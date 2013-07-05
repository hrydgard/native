#pragma once

#include "../base/basictypes.h"
#include <vector>

typedef std::pair<uint32,uint32> ExpressionPair;
typedef std::vector<ExpressionPair> PostfixExpression;

class IExpressionFunctions
{
public:
	virtual bool parseReference(char* str, uint32& referenceIndex) = 0;
	virtual bool parseSymbol(char* str, uint32& symbolValue) = 0;
	virtual uint32 getReferenceValue(uint32 referenceIndex) = 0;
	virtual bool getMemoryValue(uint32 address, int size, uint32& dest, char* error) = 0;
};

bool initPostfixExpression(char* infix, IExpressionFunctions* funcs, PostfixExpression& dest);
bool parsePostfixExpression(PostfixExpression& exp, IExpressionFunctions* funcs, uint32& dest);
bool parseExpression(char* exp, IExpressionFunctions* funcs, uint32& dest);
const char* getExpressionError();
