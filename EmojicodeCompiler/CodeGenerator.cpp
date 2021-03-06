//
//  CodeGenerator.cpp
//  Emojicode
//
//  Created by Theo Weidmann on 19/09/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#include "CodeGenerator.hpp"
#include <cstring>
#include <vector>
#include "CallableParserAndGenerator.hpp"
#include "Protocol.hpp"
#include "CallableScoper.hpp"
#include "Class.hpp"
#include "EmojicodeCompiler.hpp"
#include "StringPool.hpp"
#include "ValueType.hpp"
#include "DiscardingCallableWriter.hpp"
#include "TypeDefinitionFunctional.hpp"

template <typename T>
int writeUsed(const std::vector<T *> &functions, Writer &writer) {
    int counter = 0;
    for (auto function : functions) {
        if (function->used()) {
            writer.writeFunction(function);
            counter++;
        }
    }
    return counter;
}

template <typename T>
void compileUnused(const std::vector<T *> &functions) {
    for (auto function : functions) {
        if (!function->used() && !function->isNative()) {
            DiscardingCallableWriter writer = DiscardingCallableWriter();
            generateCodeForFunction(function, writer);
        }
    }
}
void generateCodeForFunction(Function *function, CallableWriter &w) {
    CallableScoper scoper = CallableScoper();
    if (CallableParserAndGenerator::hasInstanceScope(function->compilationMode())) {
        scoper = CallableScoper(&function->owningType().typeDefinitionFunctional()->instanceScope());
    }
    CallableParserAndGenerator::writeAndAnalyzeFunction(function, w, function->owningType().disableSelfResolving(),
                                                        scoper, function->compilationMode());
}

void writeProtocolTable(Type type, Writer &writer) {
    auto typeDefinitionFunctional = type.typeDefinitionFunctional();
    writer.writeUInt16(typeDefinitionFunctional->protocols().size());
    if (typeDefinitionFunctional->protocols().size() > 0) {
        auto biggestPlaceholder = writer.writePlaceholder<uint16_t>();
        auto smallestPlaceholder = writer.writePlaceholder<uint16_t>();

        uint_fast16_t smallestProtocolIndex = UINT_FAST16_MAX;
        uint_fast16_t biggestProtocolIndex = 0;

        for (Type protocol : typeDefinitionFunctional->protocols()) {
            writer.writeUInt16(protocol.protocol()->index);

            if (protocol.protocol()->index > biggestProtocolIndex) {
                biggestProtocolIndex = protocol.protocol()->index;
            }
            if (protocol.protocol()->index < smallestProtocolIndex) {
                smallestProtocolIndex = protocol.protocol()->index;
            }

            writer.writeUInt16(protocol.protocol()->methods().size());

            for (auto method : protocol.protocol()->methods()) {
                try {
                    Function *clm = typeDefinitionFunctional->lookupMethod(method->name());
                    if (clm == nullptr) {
                        auto typeName = type.toString(Type::nothingness(), true);
                        auto protocolName = protocol.toString(Type::nothingness(), true);
                        throw CompilerError(typeDefinitionFunctional->position(),
                                            "%s does not agree to protocol %s: Method %s is missing.",
                                            typeName.c_str(), protocolName.c_str(), method->name().utf8().c_str());
                    }

                    writer.writeUInt16(clm->vtiForUse());
                    clm->enforcePromises(method, type, protocol, TypeContext(protocol));
                }
                catch (CompilerError &ce) {
                    printError(ce);
                }
            }
        }

        biggestPlaceholder.write(biggestProtocolIndex);
        smallestPlaceholder.write(smallestProtocolIndex);
    }
}

void writeClass(Type classType, Writer &writer) {
    auto eclass = classType.eclass();

    writer.writeEmojicodeChar(eclass->name()[0]);
    if (eclass->superclass()) {
        writer.writeUInt16(eclass->superclass()->index);
    }
    else {
        // If the class does not have a superclass the own index is written.
        writer.writeUInt16(eclass->index);
    }

    writer.writeUInt16(eclass->size());
    writer.writeUInt16(eclass->fullMethodCount());
    writer.writeByte(eclass->inheritsInitializers() ? 1 : 0);
    writer.writeUInt16(eclass->fullInitializerCount());

    writer.writeUInt16(eclass->usedMethodCount());
    writer.writeUInt16(eclass->usedInitializerCount());

    writeUsed(eclass->methodList(), writer);
    writeUsed(eclass->typeMethodList(), writer);
    writeUsed(eclass->initializerList(), writer);

    writeProtocolTable(classType, writer);

    std::vector<ObjectVariableInformation> information;
    for (auto variable : eclass->instanceScope().map()) {
        variable.second.type().objectVariableRecords(variable.second.id(), information);
    }

    writer.writeUInt16(information.size());

    for (auto info : information) {
        writer.writeUInt16(info.index);
        writer.writeUInt16(info.conditionIndex);
        writer.writeUInt16(static_cast<uint16_t>(info.type));
    }
}

void writePackageHeader(Package *pkg, Writer &writer) {
    if (pkg->requiresBinary()) {
        size_t l = pkg->name().size() + 1;
        writer.writeByte(l);
        writer.writeBytes(pkg->name().c_str(), l);

        writer.writeUInt16(pkg->version().major);
        writer.writeUInt16(pkg->version().minor);
    }
    else {
        writer.writeByte(0);
    }

    writer.writeUInt16(pkg->classes().size());
}

void generateCode(Writer &writer) {
    auto &theStringPool = StringPool::theStringPool();
    theStringPool.poolString(EmojicodeString());

    ValueTypeVTIProvider provider;
    Function::start->setVtiProvider(&provider);
    Function::start->vtiForUse();

    for (auto vt : ValueType::valueTypes()) {  // Must be processed first, different sizes
        vt->finalize();
    }
    for (auto eclass : Class::classes()) {  // Can be processed afterwards, all pointers are 1 word
        eclass->finalize();
    }

    while (!Function::compilationQueue.empty()) {
        Function *function = Function::compilationQueue.front();
        generateCodeForFunction(function, function->writer_);
        Function::compilationQueue.pop();
    }

    writer.writeByte(ByteCodeSpecificationVersion);
    writer.writeUInt16(Class::classes().size());
    writer.writeUInt16(Function::functionCount());

    auto pkgCount = Package::packagesInOrder().size();

    if (pkgCount > 256) {
        throw CompilerError(Package::packagesInOrder().back()->position(), "You exceeded the maximum of 256 packages.");
    }

    writer.writeByte(pkgCount);

    for (auto pkg : Package::packagesInOrder()) {
        writePackageHeader(pkg, writer);

        for (auto cl : pkg->classes()) {
            writeClass(Type(cl, false), writer);
        }

        auto placeholder = writer.writePlaceholder<uint16_t>();
        placeholder.write(writeUsed(pkg->functions(), writer));
    }

    int smallestBoxIdentifier = UINT16_MAX;
    int biggestBoxIdentifier = 0;
    int vtWithProtocolsCount = 0;
    auto countPlaceholder = writer.writePlaceholder<uint16_t>();
    auto smallestPlaceholder = writer.writePlaceholder<uint16_t>();
    for (auto vt : ValueType::valueTypes()) {
        if (vt->protocols().size() > 0) {
            writer.writeUInt16(vt->boxIdentifier());
            writeProtocolTable(Type(vt, false, false, false), writer);
            if (vt->boxIdentifier() < smallestBoxIdentifier) {
                smallestBoxIdentifier = vt->boxIdentifier();
            }
            if (vt->boxIdentifier() > biggestBoxIdentifier) {
                biggestBoxIdentifier = vt->boxIdentifier();
            }
            vtWithProtocolsCount++;
        }
    }
    countPlaceholder.write(vtWithProtocolsCount > 0 ? biggestBoxIdentifier - smallestBoxIdentifier + 1 : 0);
    smallestPlaceholder.write(smallestBoxIdentifier);

    writer.writeUInt16(theStringPool.strings().size());
    for (auto string : theStringPool.strings()) {
        writer.writeUInt16(string.size());

        for (auto c : string) {
            writer.writeEmojicodeChar(c);
        }
    }

    for (auto eclass : Class::classes()) {
        compileUnused(eclass->methodList());
        compileUnused(eclass->initializerList());
        compileUnused(eclass->typeMethodList());
    }

    for (auto vt : ValueType::valueTypes()) {
        compileUnused(vt->methodList());
        compileUnused(vt->initializerList());
        compileUnused(vt->typeMethodList());
    }
}
