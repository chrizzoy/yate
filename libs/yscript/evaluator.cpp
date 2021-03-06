/**
 * Evaluator.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2006 Null Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "yateclass.h"
#include "yatescript.h"

#include <stdlib.h>
#include <string.h>

using namespace TelEngine;

#define MAKEOP(s,o) { s, ExpEvaluator::Opc ## o }
#define ASSIGN(s,o) { s "=", ExpEvaluator::Opc ## o | ExpEvaluator::OpcAssign }
static const TokenDict s_operators_c[] =
{
    ASSIGN("<<",Shl),
    ASSIGN(">>",Shr),
    ASSIGN("+", Add),
    ASSIGN("-", Sub),
    ASSIGN("*", Mul),
    ASSIGN("/", Div),
    ASSIGN("%", Mod),
    ASSIGN("&", And),
    ASSIGN("|", Or),
    ASSIGN("^", Xor),
    MAKEOP("<<",Shl),
    MAKEOP(">>",Shr),
    MAKEOP("==",Eq),
    MAKEOP("!=",Ne),
    MAKEOP("<=",Le),
    MAKEOP(">=",Ge),
    MAKEOP("<",Lt),
    MAKEOP(">",Gt),
    MAKEOP("&&",LAnd),
    MAKEOP("||",LOr),
    MAKEOP("^^",LXor),
    MAKEOP("+", Add),
    MAKEOP("-", Sub),
    MAKEOP("*", Mul),
    MAKEOP("/", Div),
    MAKEOP("%", Mod),
    MAKEOP("&", And),
    MAKEOP("|", Or),
    MAKEOP("^", Xor),
    MAKEOP("@", As),
    MAKEOP("=", Assign),
    { 0, 0 }
};

static const TokenDict s_unaryOps_c[] =
{
    MAKEOP("++", IncPre),
    MAKEOP("--", DecPre),
    MAKEOP("!", LNot),
    MAKEOP("~", Not),
    MAKEOP("-", Neg),
    { 0, 0 }
};

const TokenDict s_operators_sql[] =
{
    MAKEOP("AND",LAnd),
    MAKEOP("OR",LOr),
    MAKEOP("<<",Shl),
    MAKEOP(">>",Shr),
    MAKEOP("<>",Ne),
    MAKEOP("!=",Ne),
    MAKEOP("<=",Le),
    MAKEOP(">=",Ge),
    MAKEOP("<",Lt),
    MAKEOP(">",Gt),
    MAKEOP("||",Cat),
    MAKEOP("AS",As),
    MAKEOP("+", Add),
    MAKEOP("-", Sub),
    MAKEOP("*", Mul),
    MAKEOP("/", Div),
    MAKEOP("%", Mod),
    MAKEOP("&", And),
    MAKEOP("|", Or),
    MAKEOP("^", Xor),
    MAKEOP("=",Eq),
    { 0, 0 }
};

static const TokenDict s_unaryOps_sql[] =
{
    MAKEOP("NOT", LNot),
    MAKEOP("~", Not),
    MAKEOP("-", Neg),
    { 0, 0 }
};

#undef MAKEOP
#undef ASSIGN

RefObject* ExpExtender::refObj()
{
    return 0;
}

bool ExpExtender::hasField(ObjList& stack, const String& name, GenObject* context) const
{
    return false;
}

NamedString* ExpExtender::getField(ObjList& stack, const String& name, GenObject* context) const
{
    return 0;
}

bool ExpExtender::runFunction(ObjList& stack, const ExpOperation& oper, GenObject* context)
{
    return false;
}

bool ExpExtender::runField(ObjList& stack, const ExpOperation& oper, GenObject* context)
{
    return false;
}

bool ExpExtender::runAssign(ObjList& stack, const ExpOperation& oper, GenObject* context)
{
    return false;
}


ExpEvaluator::ExpEvaluator(const TokenDict* operators, const TokenDict* unaryOps)
    : m_operators(operators), m_unaryOps(unaryOps),
      m_inError(false), m_lineNo(1), m_extender(0)
{
}

ExpEvaluator::ExpEvaluator(ExpEvaluator::Parser style)
    : m_operators(0), m_unaryOps(0),
    m_inError(false), m_lineNo(1), m_extender(0)
{
    switch (style) {
	case C:
	    m_operators = s_operators_c;
	    m_unaryOps = s_unaryOps_c;
	    break;
	case SQL:
	    m_operators = s_operators_sql;
	    m_unaryOps = s_unaryOps_sql;
	    break;
    }
}

ExpEvaluator::ExpEvaluator(const ExpEvaluator& original)
    : m_operators(original.m_operators), m_unaryOps(original.unaryOps()),
      m_inError(false), m_lineNo(original.lineNumber()), m_extender(0)
{
    extender(original.extender());
    for (ObjList* l = original.m_opcodes.skipNull(); l; l = l->skipNext()) {
	const ExpOperation* o = static_cast<const ExpOperation*>(l->get());
	m_opcodes.append(o->clone());
    }
}

ExpEvaluator::~ExpEvaluator()
{
    extender(0);
}

void ExpEvaluator::extender(ExpExtender* ext)
{
    if (ext == m_extender)
	return;
    if (ext && ext->refObj() && !ext->refObj()->ref())
	return;
    ExpExtender* tmp = m_extender;
    m_extender = ext;
    if (tmp)
	TelEngine::destruct(tmp->refObj());
}

char ExpEvaluator::skipWhites(const char*& expr)
{
    if (!expr)
	return 0;
    for (; ; expr++) {
	char c = *expr;
	switch (c) {
	    case ' ':
	    case '\t':
		continue;
	    case '\r':
		m_lineNo++;
		if (expr[1] == '\n')
		    expr++;
		continue;
	    case '\n':
		m_lineNo++;
		if (expr[1] == '\r')
		    expr++;
		continue;
	    default:
		return c;
	}
    }
}

bool ExpEvaluator::keywordChar(char c) const
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
	('0' <= c && c <= '9') || (c == '_');
}

char ExpEvaluator::skipComments(const char*& expr, GenObject* context)
{
    return skipWhites(expr);
}

int ExpEvaluator::preProcess(const char*& expr, GenObject* context)
{
    return -1;
}

ExpEvaluator::Opcode ExpEvaluator::getOperator(const char*& expr, const TokenDict* operators, bool caseInsensitive) const
{
    XDebug(this,DebugAll,"getOperator('%.30s',%p,%s)",expr,operators,String::boolText(caseInsensitive));
    if (operators) {
	bool kw = keywordChar(*expr);
	for (const TokenDict* o = operators; o->token; o++) {
	    const char* s1 = o->token;
	    const char* s2 = expr;
	    do {
		if (!*s1) {
		    if (kw && keywordChar(*s2))
			break;
		    expr = s2;
		    return (ExpEvaluator::Opcode)o->value;
		}
	    } while (condLower(*s1++,caseInsensitive) == condLower(*s2++,caseInsensitive));
	}
    }
    return OpcNone;
}

bool ExpEvaluator::gotError(const char* error, const char* text, unsigned int line) const
{
    if (!error) {
	if (!text)
	    return false;
	error = "unknown error";
    }
    if (!line)
	line = lineNumber();
    String lineNo;
    formatLineNo(lineNo,line);
    Debug(this,DebugWarn,"Evaluator error: %s in %s%s%.50s",error,
	lineNo.c_str(),(text ? " at: " : ""),c_safe(text));
    return false;
}

bool ExpEvaluator::gotError(const char* error, const char* text, unsigned int line)
{
    m_inError = true;
    return const_cast<const ExpEvaluator*>(this)->gotError(error,text);
}

void ExpEvaluator::formatLineNo(String& buf, unsigned int line) const
{
    buf.clear();
    buf << "line " << line;
}

bool ExpEvaluator::getInstruction(const char*& expr, char stop, GenObject* nested)
{
    return false;
}

bool ExpEvaluator::getOperand(const char*& expr, bool endOk)
{
    if (inError())
	return false;
    XDebug(this,DebugAll,"getOperand '%.30s'",expr);
    char c = skipComments(expr);
    if (!c)
	// end of string
	return endOk;
    if (c == '(') {
	// parenthesized subexpression
	if (!runCompile(++expr,')'))
	    return false;
	if (skipComments(expr) != ')')
	    return gotError("Expecting ')'",expr);
	expr++;
	return true;
    }
    Opcode op = getUnaryOperator(expr);
    if (op != OpcNone) {
	if (!getOperand(expr,false))
	    return false;
	addOpcode(op);
	return true;
    }
    if (getSimple(expr) || getFunction(expr) || getField(expr))
	return true;
    return gotError("Expecting operand",expr);
}

bool ExpEvaluator::getSimple(const char*& expr, bool constOnly)
{
    return getString(expr) || getNumber(expr);
}

bool ExpEvaluator::getNumber(const char*& expr)
{
    if (inError())
	return false;
    XDebug(this,DebugAll,"getNumber '%.30s'",expr);
    char* endp = 0;
    long int val = ::strtol(expr,&endp,0);
    if (!endp || (endp == expr))
	return false;
    expr = endp;
    DDebug(this,DebugAll,"Found %ld",val);
    addOpcode(val);
    return true;
}

bool ExpEvaluator::getString(const char*& expr)
{
    if (inError())
	return false;
    XDebug(this,DebugAll,"getString '%.30s'",expr);
    char c = skipComments(expr);
    if (c == '"' || c == '\'') {
	String str;
	if (getString(expr,str)) {
	    addOpcode(str);
	    return true;
	}
    }
    return false;
}

bool ExpEvaluator::getString(const char*& expr, String& str)
{
    char sep = *expr++;
    const char* start = expr;
    while (char c = *expr++) {
	if (c != '\\' && c != sep)
	    continue;
	String tmp(start,expr-start-1);
	str += tmp;
	if (c == sep) {
	    DDebug(this,DebugAll,"Found '%s'",str.safe());
	    return true;
	}
	tmp.clear();
	if (!getEscape(expr,tmp,sep))
	    break;
	str += tmp;
	start = expr;
    }
    expr--;
    return gotError("Expecting string end");
}

bool ExpEvaluator::getEscape(const char*& expr, String& str, char sep)
{
    char c = *expr++;
    switch (c) {
	case '\0':
	    return false;
	case 'b':
	    c = '\b';
	    break;
	case 'f':
	    c = '\f';
	    break;
	case 'n':
	    c = '\n';
	    break;
	case 'r':
	    c = '\r';
	    break;
	case 't':
	    c = '\t';
	    break;
	case 'v':
	    c = '\v';
	    break;
    }
    str = c;
    return true;
}

int ExpEvaluator::getKeyword(const char* str) const
{
    int len = 0;
    for (;; len++) {
	char c = *str++;
	if (c <= ' ' || !keywordChar(c))
	    break;
    }
    return len;
}

bool ExpEvaluator::getFunction(const char*& expr)
{
    if (inError())
	return false;
    XDebug(this,DebugAll,"getFunction '%.30s'",expr);
    skipComments(expr);
    int len = getKeyword(expr);
    const char* s = expr+len;
    skipComments(expr);
    if ((len <= 0) || (skipComments(s) != '('))
	return false;
    s++;
    int argc = 0;
    // parameter list
    do {
	if (!runCompile(s,')')) {
	    if (!argc && (skipComments(s) == ')'))
		break;
	    return false;
	}
	argc++;
    } while (getSeparator(s,true));
    if (skipComments(s) != ')')
	return gotError("Expecting ')' after function",s);
    String str(expr,len);
    expr = s+1;
    DDebug(this,DebugAll,"Found %s()",str.safe());
    addOpcode(OpcFunc,str,argc);
    return true;
}

bool ExpEvaluator::getField(const char*& expr)
{
    if (inError())
	return false;
    XDebug(this,DebugAll,"getField '%.30s'",expr);
    skipComments(expr);
    int len = getKeyword(expr);
    if (len <= 0)
	return false;
    if (expr[len] == '(')
	return false;
    String str(expr,len);
    expr += len;
    DDebug(this,DebugAll,"Found field '%s'",str.safe());
    addOpcode(OpcField,str);
    return true;
}

ExpEvaluator::Opcode ExpEvaluator::getOperator(const char*& expr)
{
    skipComments(expr);
    return getOperator(expr,m_operators);
}

ExpEvaluator::Opcode ExpEvaluator::getUnaryOperator(const char*& expr)
{
    skipComments(expr);
    return getOperator(expr,m_unaryOps);
}

ExpEvaluator::Opcode ExpEvaluator::getPostfixOperator(const char*& expr)
{
    return OpcNone;
}

const char* ExpEvaluator::getOperator(ExpEvaluator::Opcode oper) const
{
    const char* res = lookup(oper,m_operators);
    return res ? res : lookup(oper,m_unaryOps);
}

int ExpEvaluator::getPrecedence(ExpEvaluator::Opcode oper) const
{
    switch (oper) {
	case OpcIncPre:
	case OpcDecPre:
	case OpcIncPost:
	case OpcDecPost:
	case OpcNeg:
	case OpcNot:
	    return 11;
	case OpcMul:
	case OpcDiv:
	case OpcMod:
	case OpcAnd:
	    return 10;
	case OpcAdd:
	case OpcSub:
	case OpcOr:
	case OpcXor:
	    return 9;
	case OpcShl:
	case OpcShr:
	    return 8;
	case OpcCat:
	    return 7;
	// ANY, ALL, SOME = 6
	case OpcLNot:
	    return 5;
	case OpcLt:
	case OpcGt:
	case OpcLe:
	case OpcGe:
	case OpcEq:
	case OpcNe:
	    return 4;
	// IN, BETWEEN, LIKE, MATCHES = 3
	case OpcLAnd:
	    return 2;
	case OpcLOr:
	case OpcLXor:
	    return 1;
	default:
	    return 0;
    }
}

bool ExpEvaluator::getRightAssoc(ExpEvaluator::Opcode oper) const
{
    if (oper & OpcAssign)
	return true;
    switch (oper) {
	case OpcIncPre:
	case OpcDecPre:
	case OpcNeg:
	case OpcNot:
	case OpcLNot:
	    return true;
	default:
	    return false;
    }
}

bool ExpEvaluator::getSeparator(const char*& expr, bool remove)
{
    if (skipComments(expr) != ',')
	return false;
    if (remove)
	expr++;
    return true;
}

bool ExpEvaluator::runCompile(const char*& expr, char stop, GenObject* nested)
{
    char buf[2];
    const char* stopStr = 0;
    if (stop) {
	buf[0] = stop;
	buf[1] = '\0';
	stopStr = buf;
    }
    return runCompile(expr,stopStr,nested);
}

bool ExpEvaluator::runCompile(const char*& expr, const char* stop, GenObject* nested)
{
    typedef struct {
	Opcode code;
	int prec;
    } StackedOpcode;
    StackedOpcode stack[10];
    unsigned int stackPos = 0;
#ifdef DEBUG
    Debugger debug(DebugInfo,"runCompile()"," '%s' %p '%.30s'",TelEngine::c_safe(stop),nested,expr);
#endif
    if (skipComments(expr) == ')')
	return false;
    m_inError = false;
    if (expr[0] == '*' && !expr[1]) {
	expr++;
	addOpcode(OpcField,"*");
	return true;
    }
    char stopChar = stop ? stop[0] : '\0';
    for (;;) {
	while (!stackPos && skipComments(expr) && (!stop || !::strchr(stop,*expr)) && getInstruction(expr,stopChar,nested))
	    ;
	if (inError())
	    return false;
	char c = skipComments(expr);
	if (c && stop && ::strchr(stop,c))
	    return true;
	if (!getOperand(expr))
	    return false;
	Opcode oper;
	while ((oper = getPostfixOperator(expr)) != OpcNone)
	    addOpcode(oper);
	if (inError())
	    return false;
	c = skipComments(expr);
	if (!c || (stop && ::strchr(stop,c)) || getSeparator(expr,false)) {
	    while (stackPos)
		addOpcode(stack[--stackPos].code);
	    return true;
	}
	if (inError())
	    return false;
	skipComments(expr);
	oper = getOperator(expr);
	if (oper == OpcNone)
	    return gotError("Operator or separator expected",expr);
	int precedence = 2 * getPrecedence(oper);
	int precAdj = precedence;
	// precedence being equal favor right associative operators
	if (getRightAssoc(oper))
	    precAdj++;
	while (stackPos && stack[stackPos-1].prec >= precAdj)
	    addOpcode(stack[--stackPos].code);
	if (stackPos >= (sizeof(stack)/sizeof(StackedOpcode)))
	    return gotError("Compiler stack overflow");
	stack[stackPos].code = oper;
	stack[stackPos].prec = precedence;
	stackPos++;
    }
}

bool ExpEvaluator::trySimplify()
{
    DDebug(this,DebugInfo,"trySimplify");
    bool done = false;
    for (unsigned int i = 0; ; i++) {
	ExpOperation* o = static_cast<ExpOperation*>(m_opcodes[i]);
	if (!o) {
	    if (i >= m_opcodes.length())
		break;
	    else
		continue;
	}
	if (o->barrier())
	    continue;
	switch (o->opcode()) {
	    case OpcLAnd:
	    case OpcLOr:
	    case OpcLXor:
	    case OpcAnd:
	    case OpcOr:
	    case OpcXor:
	    case OpcShl:
	    case OpcShr:
	    case OpcAdd:
	    case OpcSub:
	    case OpcMul:
	    case OpcDiv:
	    case OpcMod:
	    case OpcCat:
	    case OpcEq:
	    case OpcNe:
	    case OpcLt:
	    case OpcGt:
	    case OpcLe:
	    case OpcGe:
		if (i >= 2) {
		    ExpOperation* op2 = static_cast<ExpOperation*>(m_opcodes[i-1]);
		    ExpOperation* op1 = static_cast<ExpOperation*>(m_opcodes[i-2]);
		    if (!op1 || !op2)
			continue;
		    if (o->opcode() == OpcLAnd || o->opcode() == OpcAnd || o->opcode() == OpcMul) {
			if ((op1->opcode() == OpcPush && !op1->number() && op2->opcode() == OpcField) ||
			    (op2->opcode() == OpcPush && !op2->number() && op1->opcode() == OpcField)) {
			    ExpOperation* newOp = (o->opcode() == OpcLAnd) ? new ExpOperation(false) : new ExpOperation((long int)0);
			    newOp->lineNumber(o->lineNumber());
			    (m_opcodes+i)->set(newOp);
			    m_opcodes.remove(op1);
			    m_opcodes.remove(op2);
			    i -= 2;
			    done = true;
			    continue;
			}
		    }
		    if (o->opcode() == OpcLOr) {
			if ((op1->opcode() == OpcPush && op1->number() && op2->opcode() == OpcField) ||
			    (op2->opcode() == OpcPush && op2->number() && op1->opcode() == OpcField)) {
			    ExpOperation* newOp = new ExpOperation(true);
			    newOp->lineNumber(o->lineNumber());
			    (m_opcodes+i)->set(newOp);
			    m_opcodes.remove(op1);
			    m_opcodes.remove(op2);
			    i -= 2;
			    done = true;
			    continue;
			}
		    }
		    if ((op1->opcode() == OpcPush) && (op2->opcode() == OpcPush)) {
			ObjList stack;
			pushOne(stack,op1->clone());
			pushOne(stack,op2->clone());
			if (runOperation(stack,*o)) {
			    // replace operators and operation with computed constant
			    ExpOperation* newOp = popOne(stack);
			    newOp->lineNumber(o->lineNumber());
			    (m_opcodes+i)->set(newOp);
			    m_opcodes.remove(op1);
			    m_opcodes.remove(op2);
			    i -= 2;
			    done = true;
			}
		    }
		}
		break;
	    case OpcNeg:
	    case OpcNot:
	    case OpcLNot:
		if (i >= 1) {
		    ExpOperation* op = static_cast<ExpOperation*>(m_opcodes[i-1]);
		    if (!op)
			continue;
		    if (op->opcode() == OpcPush) {
			ObjList stack;
			pushOne(stack,op->clone());
			if (runOperation(stack,*o)) {
			    // replace unary operator and operation with computed constant
			    ExpOperation* newOp = popOne(stack);
			    newOp->lineNumber(o->lineNumber());
			    (m_opcodes+i)->set(newOp);
			    m_opcodes.remove(op);
			    i--;
			    done = true;
			}
		    }
		    else if (op->opcode() == o->opcode() && op->opcode() != OpcLNot) {
			// minus or bit negation applied twice - remove both operators
			m_opcodes.remove(o);
			m_opcodes.remove(op);
			i--;
			done = true;
		    }
		}
		break;
	    default:
		break;
	}
    }
    return done;
}

void ExpEvaluator::addOpcode(ExpOperation* oper, unsigned int line)
{
    if (!oper)
	return;
    DDebug(this,DebugAll,"addOpcode %u (%s), %u",oper->opcode(),getOperator(oper->opcode()),line);
    if (!line)
	line = lineNumber();
    oper->lineNumber(line);
    m_opcodes.append(oper);
}

ExpOperation* ExpEvaluator::addOpcode(ExpEvaluator::Opcode oper, bool barrier)
{
    DDebug(this,DebugAll,"addOpcode %u (%s)",oper,getOperator(oper));
    if (oper == OpcAs) {
	// the second operand is used just for the field name
	ExpOperation* o = 0;
	for (ObjList* l = m_opcodes.skipNull(); l; l=l->skipNext())
	    o = static_cast<ExpOperation*>(l->get());
	if (o && (o->opcode() == OpcField)) {
	    o->m_opcode = OpcPush;
	    o->String::operator=(o->name());
	}
    }
    ExpOperation* op = new ExpOperation(oper,0,ExpOperation::nonInteger(),barrier);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::addOpcode(ExpEvaluator::Opcode oper, long int value, bool barrier)
{
    DDebug(this,DebugAll,"addOpcode %u (%s) %lu",oper,getOperator(oper),value);
    ExpOperation* op = new ExpOperation(oper,0,value,barrier);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::addOpcode(ExpEvaluator::Opcode oper, const String& name, long int value, bool barrier)
{
    DDebug(this,DebugAll,"addOpcode %u (%s) '%s' %ld",oper,getOperator(oper),name.c_str(),value);
    ExpOperation* op = new ExpOperation(oper,name,value,barrier);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::addOpcode(const String& value)
{
    DDebug(this,DebugAll,"addOpcode ='%s'",value.c_str());
    ExpOperation* op = new ExpOperation(value);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::addOpcode(long int value)
{
    DDebug(this,DebugAll,"addOpcode =%ld",value);
    ExpOperation* op = new ExpOperation(value);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::addOpcode(bool value)
{
    DDebug(this,DebugAll,"addOpcode =%s",String::boolText(value));
    ExpOperation* op = new ExpOperation(value);
    op->lineNumber(lineNumber());
    m_opcodes.append(op);
    return op;
}

ExpOperation* ExpEvaluator::popOpcode()
{
    ObjList* l = &m_opcodes;
    for (ObjList* p = l; p; p = p->next()) {
	if (p->get())
	    l = p;
    }
    return static_cast<ExpOperation*>(l->remove(false));
}

unsigned int ExpEvaluator::getLineOf(ExpOperation* op1, ExpOperation* op2, ExpOperation* op3)
{
    if (op1 && op1->lineNumber())
	return op1->lineNumber();
    if (op2 && op2->lineNumber())
	return op2->lineNumber();
    if (op3 && op3->lineNumber())
	return op3->lineNumber();
    return 0;
}

void ExpEvaluator::pushOne(ObjList& stack, ExpOperation* oper)
{
    if (oper)
	stack.insert(oper);
}

ExpOperation* ExpEvaluator::popOne(ObjList& stack)
{
    ExpOperation* o = 0;
    for (;;) {
	o = static_cast<ExpOperation*>(stack.get());
	if (o || !stack.next())
	    break;
	// non-terminal NULL - remove the list entry
	stack.remove();
    }
    if (o && o->barrier()) {
	XDebug(DebugInfo,"Not popping barrier %u: '%s'='%s'",o->opcode(),o->name().c_str(),o->c_str());
	return 0;
    }
    stack.remove(o,false);
#ifdef DEBUG
#ifdef XDEBUG
    Debug(DebugAll,"popOne: %p%s%s",o,
	(YOBJECT(ExpFunction,o) ? " function" : ""),
	(YOBJECT(ExpWrapper,o) ? " wrapper" : ""));
#else
    Debug(DebugAll,"popOne: %p",o);
#endif
#endif
    return o;
}

ExpOperation* ExpEvaluator::popAny(ObjList& stack)
{
    ExpOperation* o = 0;
    for (;;) {
	o = static_cast<ExpOperation*>(stack.get());
	if (o || !stack.next())
	    break;
	// non-terminal NULL - remove the list entry
	stack.remove();
    }
    stack.remove(o,false);
#ifdef DEBUG
#ifdef XDEBUG
    Debug(DebugAll,"popAny: %p%s%s '%s'",o,
	(YOBJECT(ExpFunction,o) ? " function" : ""),
	(YOBJECT(ExpWrapper,o) ? " wrapper" : ""),
	(o ? o->name().safe() : (const char*)0));
#else
    Debug(DebugAll,"popAny: %p '%s'",o,(o ? o->name().safe() : (const char*)0));
#endif
#endif
    return o;
}

ExpOperation* ExpEvaluator::popValue(ObjList& stack, GenObject* context) const
{
    ExpOperation* oper = popOne(stack);
    if (!oper || (oper->opcode() != OpcField))
	return oper;
    XDebug(DebugAll,"ExpEvaluator::popValue() field '%s' [%p]",
	oper->name().c_str(),this);
    bool ok = runField(stack,*oper,context);
    TelEngine::destruct(oper);
    return ok ? popOne(stack) : 0;
}

bool ExpEvaluator::runOperation(ObjList& stack, const ExpOperation& oper, GenObject* context) const
{
    DDebug(this,DebugAll,"runOperation(%p,%u,%p) %s",&stack,oper.opcode(),context,getOperator(oper.opcode()));
    XDebug(this,DebugAll,"stack: %s",dump(stack).c_str());
    bool boolRes = true;
    switch (oper.opcode()) {
	case OpcPush:
	case OpcField:
	    pushOne(stack,oper.clone());
	    break;
	case OpcCopy:
	    {
		Mutex* mtx = 0;
		ScriptRun* runner = YOBJECT(ScriptRun,&oper);
		if (runner) {
		    if (runner->context())
			mtx = runner->context()->mutex();
		    if (!mtx)
			mtx = runner;
		}
		pushOne(stack,oper.copy(mtx));
	    }
	    break;
	case OpcNone:
	case OpcLabel:
	    break;
	case OpcDrop:
	    TelEngine::destruct(popOne(stack));
	    break;
	case OpcDup:
	    {
		ExpOperation* op = popValue(stack,context);
		if (!op)
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		pushOne(stack,op->clone());
		pushOne(stack,op);
	    }
	    break;
	case OpcAnd:
	case OpcOr:
	case OpcXor:
	case OpcShl:
	case OpcShr:
	case OpcAdd:
	case OpcSub:
	case OpcMul:
	case OpcDiv:
	case OpcMod:
	    boolRes = false;
	    // fall through
	case OpcEq:
	case OpcNe:
	case OpcLt:
	case OpcGt:
	case OpcLe:
	case OpcGe:
	    {
		ExpOperation* op2 = popValue(stack,context);
		ExpOperation* op1 = popValue(stack,context);
		if (!op1 || !op2) {
		    TelEngine::destruct(op1);
		    TelEngine::destruct(op2);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		switch (oper.opcode()) {
		    case OpcDiv:
		    case OpcMod:
			if (!op2->valInteger())
			    return gotError("Division by zero",oper.lineNumber());
		    case OpcAdd:
			if (op1->isInteger() && op2->isInteger())
			    break;
			// turn addition into concatenation
			{
			    String val = *op1 + *op2;
			    TelEngine::destruct(op1);
			    TelEngine::destruct(op2);
			    DDebug(this,DebugAll,"String result: '%s'",val.c_str());
			    pushOne(stack,new ExpOperation(val));
			    return true;
			}
		    default:
			break;
		}
		long int val = 0;
		switch (oper.opcode()) {
		    case OpcAnd:
			val = op1->valInteger() & op2->valInteger();
			break;
		    case OpcOr:
			val = op1->valInteger() | op2->valInteger();
			break;
		    case OpcXor:
			val = op1->valInteger() ^ op2->valInteger();
			break;
		    case OpcShl:
			val = op1->valInteger() << op2->valInteger();
			break;
		    case OpcShr:
			val = op1->valInteger() >> op2->valInteger();
			break;
		    case OpcAdd:
			val = op1->valInteger() + op2->valInteger();
			break;
		    case OpcSub:
			val = op1->valInteger() - op2->valInteger();
			break;
		    case OpcMul:
			val = op1->valInteger() * op2->valInteger();
			break;
		    case OpcDiv:
			val = op1->valInteger() / op2->valInteger();
			break;
		    case OpcMod:
			val = op1->valInteger() % op2->valInteger();
			break;
		    case OpcLt:
			val = (op1->valInteger() < op2->valInteger()) ? 1 : 0;
			break;
		    case OpcGt:
			val = (op1->valInteger() > op2->valInteger()) ? 1 : 0;
			break;
		    case OpcLe:
			val = (op1->valInteger() <= op2->valInteger()) ? 1 : 0;
			break;
		    case OpcGe:
			val = (op1->valInteger() >= op2->valInteger()) ? 1 : 0;
			break;
		    case OpcEq:
			val = (*op1 == *op2) ? 1 : 0;
			break;
		    case OpcNe:
			val = (*op1 != *op2) ? 1 : 0;
			break;
		    default:
			break;
		}
		TelEngine::destruct(op1);
		TelEngine::destruct(op2);
		if (boolRes) {
		    DDebug(this,DebugAll,"Bool result: '%s'",String::boolText(val != 0));
		    pushOne(stack,new ExpOperation(val != 0));
		}
		else {
		    DDebug(this,DebugAll,"Numeric result: %lu",val);
		    pushOne(stack,new ExpOperation(val));
		}
	    }
	    break;
	case OpcLAnd:
	case OpcLOr:
	    {
		ExpOperation* op2 = popValue(stack,context);
		ExpOperation* op1 = popValue(stack,context);
		if (!op1 || !op2) {
		    TelEngine::destruct(op1);
		    TelEngine::destruct(op2);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		bool val = false;
		switch (oper.opcode()) {
		    case OpcLAnd:
			val = op1->valBoolean() && op2->valBoolean();
			break;
		    case OpcLOr:
			val = op1->valBoolean() || op2->valBoolean();
			break;
		    default:
			break;
		}
		TelEngine::destruct(op1);
		TelEngine::destruct(op2);
		DDebug(this,DebugAll,"Bool result: '%s'",String::boolText(val));
		pushOne(stack,new ExpOperation(val));
	    }
	    break;
	case OpcCat:
	    {
		ExpOperation* op2 = popValue(stack,context);
		ExpOperation* op1 = popValue(stack,context);
		if (!op1 || !op2) {
		    TelEngine::destruct(op1);
		    TelEngine::destruct(op2);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		String val = *op1 + *op2;
		TelEngine::destruct(op1);
		TelEngine::destruct(op2);
		DDebug(this,DebugAll,"String result: '%s'",val.c_str());
		pushOne(stack,new ExpOperation(val));
	    }
	    break;
	case OpcAs:
	    {
		ExpOperation* op2 = popOne(stack);
		ExpOperation* op1 = popOne(stack);
		if (!op1 || !op2) {
		    TelEngine::destruct(op1);
		    TelEngine::destruct(op2);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		pushOne(stack,op1->clone(*op2));
		TelEngine::destruct(op1);
		TelEngine::destruct(op2);
	    }
	    break;
	case OpcNeg:
	case OpcNot:
	case OpcLNot:
	    {
		ExpOperation* op = popValue(stack,context);
		if (!op)
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		switch (oper.opcode()) {
		    case OpcNeg:
			pushOne(stack,new ExpOperation(-op->valInteger()));
			break;
		    case OpcNot:
			pushOne(stack,new ExpOperation(~op->valInteger()));
			break;
		    case OpcLNot:
			pushOne(stack,new ExpOperation(!op->valBoolean()));
			break;
		    default:
			pushOne(stack,new ExpOperation(op->valInteger()));
			break;
		}
		TelEngine::destruct(op);
	    }
	    break;
	case OpcFunc:
	    return runFunction(stack,oper,context) ||
		gotError("Function '" + oper.name() + "' call failed",oper.lineNumber());
	case OpcIncPre:
	case OpcDecPre:
	case OpcIncPost:
	case OpcDecPost:
	    {
		ExpOperation* fld = popOne(stack);
		if (!fld)
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		if (fld->opcode() != OpcField) {
		    TelEngine::destruct(fld);
		    return gotError("Expecting LValue in operator",oper.lineNumber());
		}
		ExpOperation* val = 0;
		if (!(runField(stack,*fld,context) && (val = popOne(stack)))) {
		    TelEngine::destruct(fld);
		    return false;
		}
		long int num = val->valInteger();
		switch (oper.opcode()) {
		    case OpcIncPre:
			num++;
			(*val) = num;
			break;
		    case OpcDecPre:
			num--;
			(*val) = num;
			break;
		    case OpcIncPost:
			(*val) = num;
			num++;
			break;
		    case OpcDecPost:
			(*val) = num;
			num--;
			break;
		    default:
			break;
		}
		(*fld) = num;
		bool ok = runAssign(stack,*fld,context);
		TelEngine::destruct(fld);
		if (!ok) {
		    TelEngine::destruct(val);
		    return gotError("Assignment failed",oper.lineNumber());
		}
		pushOne(stack,val);
	    }
	    break;
	case OpcAssign:
	    {
		ExpOperation* val = popValue(stack,context);
		ExpOperation* fld = popOne(stack);
		if (!fld || !val) {
		    TelEngine::destruct(fld);
		    TelEngine::destruct(val);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		if (fld->opcode() != OpcField) {
		    TelEngine::destruct(fld);
		    TelEngine::destruct(val);
		    return gotError("Expecting LValue in assignment",oper.lineNumber());
		}
		ExpOperation* op = val->clone(fld->name());
		TelEngine::destruct(fld);
		bool ok = runAssign(stack,*op,context);
		TelEngine::destruct(op);
		if (!ok) {
		    TelEngine::destruct(val);
		    return gotError("Assignment failed",oper.lineNumber());
		}
		pushOne(stack,val);
	    }
	    break;
	default:
	    if (oper.opcode() & OpcAssign) {
		// assignment by operation
		ExpOperation* val = popValue(stack,context);
		ExpOperation* fld = popOne(stack);
		if (!fld || !val) {
		    TelEngine::destruct(fld);
		    TelEngine::destruct(val);
		    return gotError("ExpEvaluator stack underflow",oper.lineNumber());
		}
		if (fld->opcode() != OpcField) {
		    TelEngine::destruct(fld);
		    TelEngine::destruct(val);
		    return gotError("Expecting LValue in assignment",oper.lineNumber());
		}
		pushOne(stack,fld->clone());
		pushOne(stack,fld);
		pushOne(stack,val);
		ExpOperation op((Opcode)(oper.opcode() & ~OpcAssign),
		    oper.name(),oper.number(),oper.barrier());
		if (!runOperation(stack,op,context))
		    return false;
		static const ExpOperation assign(OpcAssign);
		return runOperation(stack,assign,context);
	    }
	    Debug(this,DebugStub,"Please implement operation %u '%s'",
		oper.opcode(),getOperator(oper.opcode()));
	    return false;
    }
    return true;
}

bool ExpEvaluator::runFunction(ObjList& stack, const ExpOperation& oper, GenObject* context) const
{
    DDebug(this,DebugAll,"runFunction(%p,'%s' %ld, %p) ext=%p",
	&stack,oper.name().c_str(),oper.number(),context,(void*)m_extender);
    if (oper.name() == YSTRING("chr")) {
	String res;
	for (long int i = oper.number(); i; i--) {
	    ExpOperation* o = popValue(stack,context);
	    if (!o)
		return gotError("ExpEvaluator stack underflow",oper.lineNumber());
	    res = String((char)o->number()) + res;
	    TelEngine::destruct(o);
	}
	pushOne(stack,new ExpOperation(res));
	return true;
    }
    if (oper.name() == YSTRING("now")) {
	if (oper.number())
	    return gotError("Function expects no arguments",oper.lineNumber());
	pushOne(stack,new ExpOperation((long int)Time::secNow()));
	return true;
    }
    return m_extender && m_extender->runFunction(stack,oper,context);
}

bool ExpEvaluator::runField(ObjList& stack, const ExpOperation& oper, GenObject* context) const
{
    DDebug(this,DebugAll,"runField(%p,'%s',%p) ext=%p",
	&stack,oper.name().c_str(),context,(void*)m_extender);
    return m_extender && m_extender->runField(stack,oper,context);
}

bool ExpEvaluator::runAssign(ObjList& stack, const ExpOperation& oper, GenObject* context) const
{
    DDebug(this,DebugAll,"runAssign('%s'='%s',%p) ext=%p",
	oper.name().c_str(),oper.c_str(),context,(void*)m_extender);
    return m_extender && m_extender->runAssign(stack,oper,context);
}

bool ExpEvaluator::runEvaluate(const ObjList& opcodes, ObjList& stack, GenObject* context) const
{
    DDebug(this,DebugInfo,"runEvaluate(%p,%p,%p)",&opcodes,&stack,context);
    for (const ObjList* l = opcodes.skipNull(); l; l = l->skipNext()) {
	const ExpOperation* o = static_cast<const ExpOperation*>(l->get());
	if (!runOperation(stack,*o,context))
	    return false;
    }
    return true;
}

bool ExpEvaluator::runEvaluate(const ObjVector& opcodes, ObjList& stack, GenObject* context, unsigned int index) const
{
    DDebug(this,DebugInfo,"runEvaluate(%p,%p,%p,%u)",&opcodes,&stack,context,index);
    for (; index < opcodes.length(); index++) {
	const ExpOperation* o = static_cast<const ExpOperation*>(opcodes[index]);
	if (o && !runOperation(stack,*o,context))
	    return false;
    }
    return true;
}

bool ExpEvaluator::runEvaluate(ObjList& stack, GenObject* context) const
{
    return runEvaluate(m_opcodes,stack,context);
}

bool ExpEvaluator::runAllFields(ObjList& stack, GenObject* context) const
{
    DDebug(this,DebugAll,"runAllFields(%p,%p)",&stack,context);
    bool ok = true;
    for (ObjList* l = stack.skipNull(); l; l = l->skipNext()) {
	const ExpOperation* o = static_cast<const ExpOperation*>(l->get());
	if (o->barrier())
	    break;
	if (o->opcode() != OpcField)
	    continue;
	ObjList tmp;
	if (runField(tmp,*o,context)) {
	    ExpOperation* val = popOne(tmp);
	    if (val)
		l->set(val);
	    else
		ok = false;
	}
	else
	    ok = false;
    }
    return ok;
}

int ExpEvaluator::compile(const char* expr, GenObject* context)
{
    if (!skipComments(expr,context))
	return 0;
    int res = 0;
    for (;;) {
	int pre;
	m_inError = false;
	while ((pre = preProcess(expr,context)) >= 0)
	    res += pre;
	if (inError())
	    return 0;
	if (!runCompile(expr))
	    return 0;
	res++;
	bool sep = false;
	while (getSeparator(expr,true))
	    sep = true;
	if (!sep)
	    break;
    }
    return skipComments(expr,context) ? 0 : res;
}

bool ExpEvaluator::evaluate(ObjList* results, GenObject* context) const
{
    if (results) {
	results->clear();
	return runEvaluate(*results,context) &&
	    (runAllFields(*results,context) || gotError("Could not evaluate all fields"));
    }
    ObjList res;
    return runEvaluate(res,context);
}

int ExpEvaluator::evaluate(NamedList& results, unsigned int index, const char* prefix, GenObject* context) const
{
    ObjList stack;
    if (!evaluate(stack,context))
	return -1;
    String idx(prefix);
    if (index)
	idx << index << ".";
    int column = 0;
    for (ObjList* r = stack.skipNull(); r; r = r->skipNext()) {
	column++;
	const ExpOperation* res = static_cast<const ExpOperation*>(r->get());
	String name = res->name();
	if (name.null())
	    name = column;
	results.setParam(idx+name,*res);
    }
    return column;
}

int ExpEvaluator::evaluate(Array& results, unsigned int index, GenObject* context) const
{
    Debug(this,DebugStub,"Please implement ExpEvaluator::evaluate(Array)");
    return -1;
}

void ExpEvaluator::dump(const ExpOperation& oper, String& res) const
{
    const char* name = getOperator(oper.opcode());
    if (name) {
	res << name;
	return;
    }
    switch (oper.opcode()) {
	case OpcPush:
	case OpcCopy:
	    if (oper.isInteger())
		res << (int)oper.number();
	    else
		res << "'" << oper << "'";
	    break;
	case OpcField:
	    res << oper.name();
	    break;
	case OpcFunc:
	    res << oper.name() << "(" << (int)oper.number() << ")";
	    break;
	default:
	    res << "[" << oper.opcode() << "]";
	    if (oper.number() && oper.isInteger())
		res << "(" << (int)oper.number() << ")";
    }
}

void ExpEvaluator::dump(const ObjList& codes, String& res) const
{
    for (const ObjList* l = codes.skipNull(); l; l = l->skipNext()) {
	if (res)
	    res << " ";
	const ExpOperation* o = static_cast<const ExpOperation*>(l->get());
	dump(*o,res);
    }
}

long int ExpOperation::valInteger() const
{
    return isInteger() ? number() : 0;
}

bool ExpOperation::valBoolean() const
{
    return isInteger() ? (number() != 0) : !null();
}

ExpOperation* ExpOperation::clone(const char* name) const
{
    ExpOperation* op = new ExpOperation(*this,name);
    op->lineNumber(lineNumber());
    return op;
}


ExpOperation* ExpFunction::clone(const char* name) const
{
    XDebug(DebugInfo,"ExpFunction::clone('%s') [%p]",name,this);
    ExpFunction* op = new ExpFunction(name,number());
    op->lineNumber(lineNumber());
    return op;
}


ExpOperation* ExpWrapper::clone(const char* name) const
{
    RefObject* r = YOBJECT(RefObject,object());
    XDebug(DebugInfo,"ExpWrapper::clone('%s') %s=%p [%p]",
	name,(r ? "ref" : "obj"),object(),this);
    if (r)
	r->ref();
    ExpWrapper* op = new ExpWrapper(object(),name);
    static_cast<String&>(*op) = *this;
    op->lineNumber(lineNumber());
    return op;
}

ExpOperation* ExpWrapper::copy(Mutex* mtx) const
{
    JsObject* jso = YOBJECT(JsObject,m_object);
    if (!jso)
	return ExpOperation::clone();
    XDebug(DebugInfo,"ExpWrapper::copy(%p) [%p]",mtx,this);
    ExpWrapper* op = new ExpWrapper(jso->copy(mtx),name());
    static_cast<String&>(*op) = *this;
    op->lineNumber(lineNumber());
    return op;
}

bool ExpWrapper::valBoolean() const
{
    if (!m_object)
	return false;
    return !JsParser::isNull(*this);
}

void* ExpWrapper::getObject(const String& name) const
{
    if (name == YSTRING("ExpWrapper"))
	return const_cast<ExpWrapper*>(this);
    void* obj = ExpOperation::getObject(name);
    if (obj)
	return obj;
    return m_object ? m_object->getObject(name) : 0;
}


TableEvaluator::TableEvaluator(const TableEvaluator& original)
    : m_select(original.m_select), m_where(original.m_where),
      m_limit(original.m_limit), m_limitVal(original.m_limitVal)
{
    extender(original.m_select.extender());
}

TableEvaluator::TableEvaluator(ExpEvaluator::Parser style)
    : m_select(style), m_where(style),
      m_limit(style), m_limitVal((unsigned int)-2)
{
}

TableEvaluator::TableEvaluator(const TokenDict* operators, const TokenDict* unaryOps)
    : m_select(operators,unaryOps), m_where(operators,unaryOps),
      m_limit(operators,unaryOps), m_limitVal((unsigned int)-2)
{
}

TableEvaluator::~TableEvaluator()
{
}

void TableEvaluator::extender(ExpExtender* ext)
{
    m_select.extender(ext);
    m_where.extender(ext);
    m_limit.extender(ext);
}

bool TableEvaluator::evalWhere(GenObject* context)
{
    if (m_where.null())
	return true;
    ObjList res;
    if (!m_where.evaluate(res,context))
	return false;
    ObjList* first = res.skipNull();
    if (!first)
	return false;
    const ExpOperation* o = static_cast<const ExpOperation*>(first->get());
    return (o->opcode() == ExpEvaluator::OpcPush) && o->number();
}

bool TableEvaluator::evalSelect(ObjList& results, GenObject* context)
{
    if (m_select.null())
	return false;
    return m_select.evaluate(results,context);
}

unsigned int TableEvaluator::evalLimit(GenObject* context)
{
    if (m_limitVal == (unsigned int)-2) {
	m_limitVal = (unsigned int)-1;
	// hack: use a loop so we can break out of it
	while (!m_limit.null()) {
	    ObjList res;
	    if (!m_limit.evaluate(res,context))
		break;
	    ObjList* first = res.skipNull();
	    if (!first)
		break;
	    const ExpOperation* o = static_cast<const ExpOperation*>(first->get());
	    if (o->opcode() != ExpEvaluator::OpcPush)
		break;
	    int lim = o->number();
	    if (lim < 0)
		lim = 0;
	    m_limitVal = lim;
	    break;
	}
    }
    return m_limitVal;
}

/* vi: set ts=8 sw=4 sts=4 noet: */
