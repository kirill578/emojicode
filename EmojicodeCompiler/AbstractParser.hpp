//
//  AbstractParser.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 27/04/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#ifndef AbstractParser_hpp
#define AbstractParser_hpp

#include "Token.hpp"
#include "TokenStream.hpp"
#include "Package.hpp"
#include "Function.hpp"

struct ParsedTypeName {
    ParsedTypeName(EmojicodeString name, EmojicodeString ns, bool optional, const Token& token)
        : name(name), ns(ns), optional(optional), token(token) {}
    EmojicodeString name;
    EmojicodeString ns;
    bool optional;
    const Token& token;
};

class AbstractParser {
protected:
    AbstractParser(Package *pkg, TokenStream stream) : package_(pkg), stream_(stream) {};
    Package *package_;
    TokenStream stream_;

    /** Reads a type name and stores it into the given pointers. */
    ParsedTypeName parseTypeName();
    /** Reads a type name and stores it into the given pointers. */
    Type parseTypeDeclarative(TypeContext tc, TypeDynamism dynamism, Type expectation = Type::nothingness(),
                              TypeDynamism *dynamicType = nullptr, bool allowProtocolsUsingSelf = false);

    /**
     * Parses the arguments for a callable.
     * @return Whether self was used.
     */
    bool parseArgumentList(Callable *c, TypeContext ct, bool initializer = false);
    /**
     * Parses the return type for this function if there is one specified.
     * @return Whether self was used.
     */
    bool parseReturnType(Callable *c, TypeContext ct);
    void parseGenericArgumentsInDefinition(Function *p, TypeContext ct);
    void parseBody(Callable *p);
    void parseBody(Function *p, bool allowNative);
    void parseGenericArgumentsForType(Type *type, TypeContext ct, TypeDynamism dynamism, const Token& errorToken);
};

#endif /* AbstractParser_hpp */
