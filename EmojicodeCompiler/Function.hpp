//
//  Function.h
//  Emojicode
//
//  Created by Theo Weidmann on 04/01/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#ifndef Function_hpp
#define Function_hpp

#include <queue>
#include <map>
#include <vector>
#include <experimental/optional>
#include "Token.hpp"
#include "TokenStream.hpp"
#include "Type.hpp"
#include "Callable.hpp"
#include "CallableParserAndGeneratorMode.hpp"
#include "CallableWriter.hpp"
#include "Class.hpp"

class VTIProvider;

enum class AccessLevel {
    Public, Private, Protected
};

class Closure: public Callable {
public:
    Closure(SourcePosition p) : Callable(p) {};
};

struct FunctionObjectVariableInformation : public ObjectVariableInformation {
    FunctionObjectVariableInformation(int index, ObjectVariableType type, InstructionCount from, InstructionCount to)
        : ObjectVariableInformation(index, type), from(from), to(to) {}
    FunctionObjectVariableInformation(int index, int condition, ObjectVariableType type, InstructionCount from,
                                      InstructionCount to)
        : ObjectVariableInformation(index, condition, type), from(from), to(to) {}
    int from;
    int to;
};

/** Functions are callables that belong to a class or value type as either method, type method or initializer. */
class Function: public Callable {
    friend void Class::finalize();
    friend Protocol;
    friend void generateCode(Writer &writer);
public:
    static bool foundStart;
    static Function *start;
    static std::queue<Function *> compilationQueue;
    /** Returns a VTI for a function. */
    static int nextFunctionVti() { return nextVti_++; }
    /** Returns the number of funciton VTIs assigned. This should be equal to the number of compiled functions. */
    static int functionCount() { return nextVti_; }

    Function(EmojicodeString name, AccessLevel level, bool final, Type owningType, Package *package, SourcePosition p,
             bool overriding, EmojicodeString documentationToken, bool deprecated, bool mutating,
             CallableParserAndGeneratorMode mode)
    : Callable(p),
    name_(name),
    final_(final),
    overriding_(overriding),
    deprecated_(deprecated),
    mutating_(mutating),
    access_(level),
    owningType_(owningType),
    package_(package),
    documentation_(documentationToken),
    compilationMode_(mode) {}

    EmojicodeString name() const { return name_; }

    /** Whether the method is implemented natively and Run-Time Native Linking must occur. */
    bool isNative() const { return linkingTableIndex_ > 0; }
    unsigned int linkingTabelIndex() const { return linkingTableIndex_; }
    void setLinkingTableIndex(int index);
    /** Whether the method was marked as final and can’t be overriden. */
    bool final() const { return final_; }
    /** Whether the method is intended to override a super method. */
    bool overriding() const { return overriding_; }
    /** Whether the method is deprecated. */
    bool deprecated() const { return deprecated_; }
    /** Returns the access level to this method. */
    AccessLevel accessLevel() const { return access_; }

    /** Type to which this function belongs.
     This can be Nothingness if the function doesn’t belong to any type (e.g. 🏁). */
    Type owningType() const { return owningType_; }

    const EmojicodeString& documentation() const { return documentation_; }

    /** The types for the generic arguments. */
    std::vector<Type> genericArgumentConstraints;
    /** Generic type arguments as variables */
    std::map<EmojicodeString, Type> genericArgumentVariables;

    /** The namespace in which the function was defined.
     This does not necessarily match the package of @c owningType. */
    Package* package() const { return package_; }

    /// Issues a warning at the given position if the function is deprecated.
    void deprecatedWarning(SourcePosition position) const;

    /// Returns true if the method is validly overriding a method or false if it does not override.
    /// @throws CompilerError if the override is improper, e.g. implicit
    bool checkOverride(Function *superFunction) const;
    /// Makes this method properly inherit from @c super.
    void override(Function *super, Type superSource, Type typeContext) {
        enforcePromises(super, typeContext, superSource, std::experimental::nullopt);
        setVti(super->getVti());
        super->registerOverrider(this);
    }
    /// Checks that no promises were broken and applies boxing if necessary.
    void enforcePromises(Function *superFunction, TypeContext typeContext, Type superSource,
                         std::experimental::optional<TypeContext> protocol);

    void registerOverrider(Function *f) { overriders_.push_back(f); }

    /** Returns the VTI for this function or fetches one by calling the VTI Assigner and marks the function as used.
     @warning This method must only be called if the function will be needed at run-time and
     should be assigned a VTI. */
    int vtiForUse();
    /// Assigns this method a VTI without marking it as used.
    void assignVti();
    /// Returns the VTI this function was assigned.
    /// @throws std::logic_error if the function wasn’t assigned a VTI
    int getVti() const;
    /** Sets the @c VTIProvider which should be used to assign this method a VTI and to update the VTI counter. */
    void setVtiProvider(VTIProvider *provider);
    /// Whether the function was used.
    bool used() const { return used_; }
    /// Whether the function was assigned a VTI
    bool assigned() const;

    /// Whether the function mutates the callee. Only relevant for value type instance methods.
    bool mutating() const { return mutating_; }

    CallableParserAndGeneratorMode compilationMode() const { return compilationMode_; }

    int fullSize() const { return fullSize_; }
    void setFullSize(int c) { fullSize_ = c; }

    CallableWriter writer_;
    std::vector<FunctionObjectVariableInformation>& objectVariableInformation() { return objectVariableInformation_; }
private:
    /** Sets the VTI to @c vti and enters this functions into the list of functions to be compiled into the binary. */
    void setVti(int vti);
    void markUsed();

    EmojicodeString name_;
    static int nextVti_;
    int vti_ = -1;
    bool final_;
    bool overriding_;
    bool deprecated_;
    bool mutating_;
    bool used_ = false;
    unsigned int linkingTableIndex_ = 0;
    AccessLevel access_;
    Type owningType_;
    Package *package_;
    EmojicodeString documentation_;
    VTIProvider *vtiProvider_ = nullptr;
    CallableParserAndGeneratorMode compilationMode_;
    int fullSize_ = -1;
    std::vector<Function*> overriders_;
    std::vector<FunctionObjectVariableInformation> objectVariableInformation_;
};

class Initializer: public Function {
public:
    Initializer(EmojicodeString name, AccessLevel level, bool final, Type owningType, Package *package,
                SourcePosition p, bool overriding, EmojicodeString documentationToken, bool deprecated, bool r,
                bool crn, CallableParserAndGeneratorMode mode)
    : Function(name, level, final, owningType, package, p, overriding, documentationToken, deprecated, true, mode),
    required(r),
    canReturnNothingness(crn) {
        returnType = owningType;
    }

    bool required;
    bool canReturnNothingness;

    Type type() const override;

    void addArgumentToVariable(const EmojicodeString &string) { argumentsToVariables_.push_back(string); }
    const std::vector<EmojicodeString>& argumentsToVariables() const { return argumentsToVariables_; }
private:
    std::vector<EmojicodeString> argumentsToVariables_;
};

#endif /* Function_hpp */
