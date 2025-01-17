#The MIT License (MIT)
#
#Copyright (c) 2014, Sam Bayless
#
#Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
#associated documentation files (the "Software"), to deal in the Software without restriction,
#including without limitation the rights to use, copy, modify, merge, publish, distribute,
#sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all copies or
#substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
#NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
#DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
#OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import monosat.monosat_c     
from tempfile import mktemp
import collections
import functools
import math
import platform
import re
import os

_monosat = monosat.monosat_c.Monosat()

def getSymbols():
    return _monosat.symbolmap    

class Var:
    
    def __init__(self,symbol=None, allow_simplification=False):
        #Warning: if allow_simplification is set to true, then the variable may be eliminated by the solver at the next Solve();
        #After that point, the variable would no longer be safe to use in clauses or constraints in subsequent calls to Solve(). 
        #Use carefully!
        if isinstance(symbol,bool):  
            self.lit = _monosat.true() if symbol else _monosat.false()
        elif isinstance(symbol, int):           
            self.lit=symbol     
        elif isinstance(symbol, Var):           
            self.lit=symbol.lit
        else:            
            self.lit = _monosat.newLit(allow_simplification);
            if symbol is not None:
                _monosat.setSymbol(self.lit,str(symbol))

    def __repr__(self):
        if self.isConstTrue():
            return "T"
        elif self.isConstFalse():
            return "F"
        else:
            #return str(self.getInputLiteral())
            return str(self.getLit())
            """if(self.lit%2==0):
                return str((self.lit>>1))
            else:
                return "-"+str((self.lit>>1))"""
        
    def getSymbol(self):
        return self.symbol
    
    def setSymbol(self,s):
        pass
        #if self.symbol is not None and self.symbol in _monosat.symbolmap:
        #    del _monosat.symbolmap[self.symbol]
        #if s is not None:
        #    self.symbol = str(s)        
        #    _monosat.symbolmap[self.symbol]=self
            
    def __hash__(self):
        return self.lit          
        
    def getLit(self):
        return self.lit
    
    def value(self):
        #0 = true, 1=false, 2=unassigned
        #return _monosat.getModel_Literal(self.lit) ==0 #Treat unassigned as false
        v =  _monosat.getModel_Literal(self.lit)
        if v==0:
            return True
        elif v==1:
            return False
        else:
            return None
 
    def isConst(self):
        return self.isConstTrue() or self.isConstFalse()
    
    def isConstTrue(self):
        return self.getLit()==_monosat.true()
    
    def isConstFalse(self):
        return self.getLit()==_monosat.false()
 
    def __and__(self,other):    
        o=VAR(other)
        if(self.isConstFalse() or o.isConstFalse()):
            return false
        
        if(self.isConstTrue()):
            return o;
        if(o.isConstTrue()):
            return self;
        v=Var()
        _monosat.addTertiaryClause(v.getLit(),_monosat.Not(self.getLit()),_monosat.Not(o.getLit()))
        _monosat.addBinaryClause(_monosat.Not(v.getLit()),self.getLit())
        _monosat.addBinaryClause(_monosat.Not(v.getLit()),o.getLit())
        return v
        #return Var(_monosat.addAnd( self.getLit(),o.getLit()))
     
    def __or__(self,other):
        o=VAR(other)
        if(self.isConstTrue() or o.isConstTrue()):
            return true
        
        if(self.isConstFalse()):
            return o;
        if(o.isConstFalse()):
            return self;
        v=Var()
        _monosat.addTertiaryClause(_monosat.Not( v.getLit()),self.getLit(),o.getLit())
        _monosat.addBinaryClause(v.getLit(),_monosat.Not(self.getLit()))
        _monosat.addBinaryClause(v.getLit(),_monosat.Not(o.getLit()))
    
        return v
        #return Var()
        #return Var(_monosat.Not( _monosat.addAnd(  _monosat.Not( self.getLit()),_monosat.Not(o.getLit()))))

    def nor(self,other):
        return ~(self.__or__(other))
        """o=VAR(other)
        if(self.isConstTrue() or o.isConstTrue()):
            return false
        
        if(self.isConstFalse()):
            return o.Not();
        if(o.isConstFalse()):
            return self.Not();"""
        
        #return Var( _monosat.addAnd(  _monosat.Not( self.getLit()),_monosat.Not(o.getLit())))
        
    def nand(self,other):
        return ~(self.__and__(other))
        """o=VAR(other)
        if(self.isConstFalse() or o.isConstFalse()):
            return true
        
        if(self.isConstTrue()):
            return o.Not();
        if(o.isConstTrue()):
            return self.Not();
        return Var(_monosat.Not(_monosat.addAnd( self.getLit(),o.getLit())))"""
 
    def __invert__(self):
        return self.Not()
 
    def Not(self):
        return Var(_monosat.Not(self.getLit())) 
    
    def __eq__(self,other):                
        return self.xnor(other)
    
    def __ne__(self,other):
        return self.__xor__(other)
    
    def __xor__(self,other):
        o=VAR(other)
        
        if(self.isConst() and o.isConst()):
            return true if self.getLit() != o.getLit() else false
        
        if(self.isConstTrue()):
            return o.Not();
        
        if(self.isConstFalse()):
            return o;
        
        if(o.isConstTrue()):
            return self.Not();
        
        if(o.isConstFalse()):
            return self;
        
        v = Var()
        
        #If both inputs 0, output must be 0. 
        _monosat.addTertiaryClause(_monosat.Not( v.getLit()),self.getLit(),o.getLit())
        
        #If one in put 1, the other input 0, output must be 1. 
        _monosat.addTertiaryClause(v.getLit(),_monosat.Not( self.getLit()),o.getLit())
        
        #If one in put 1, the other input 0, output must be 1. 
        _monosat.addTertiaryClause(v.getLit(), self.getLit(),_monosat.Not(o.getLit()))
        
        # If both inputs 1, output must be 0.
        _monosat.addTertiaryClause(_monosat.Not(v.getLit()), _monosat.Not(self.getLit()),_monosat.Not(o.getLit()))
        
        #a=_monosat.addAnd(self.getLit(),_monosat.Not( o.getLit()))
        #b=_monosat.addAnd(_monosat.Not(self.getLit()),o.getLit())
        
        #return Var(_monosat.Not(_monosat.addAnd(_monosat.Not( a),_monosat.Not(b))))
        return v

    def xnor(self,other):
        return ~(self.__xor__(other))
        """ o=VAR(other)
        
        if(self.isConst() and o.isConst()):
            return true if self.getLit() == o.getLit() else false
        
        if(self.isConstFalse()):
            return o.Not();
        
        if(self.isConstTrue()):
            return o;
        
        if(o.isConstFalse()):
            return self.Not();
        
        if(o.isConstTrue()):
            return self;
        
        
        a=_monosat.addAnd(self.getLit(),_monosat.Not( o.getLit()))
        b=_monosat.addAnd(_monosat.Not(self.getLit()),o.getLit())
        
        return Var(_monosat.addAnd(_monosat.Not( a),_monosat.Not(b)))        """
        
    def implies(self,other):
        #return ~(self.__and__(~other))
        return Var(_monosat.Not(_monosat.addAnd( self.getLit(),_monosat.Not(VAR(other).getLit()) )))
    


true = Var(_monosat.true())
false = Var(_monosat.false())

#for internal use only
def _addClause(args):
    _monosat.addClause([VAR(x).getLit() for x in args])

def _addSafeClause(args):
    _monosat.addClause([x.getLit() for x in args])

def VAR(a):
    assert(a is not None)
    if (isinstance(a, Var)):
        return a;
    else:
        if a:
            return true
        else:
            return false
    

def Ite(i,t,e):
    ni = VAR(i)
    nt = VAR(t)
    ne = VAR(e)
    
    if isTrue(ni):
        return t
    elif isFalse(ni):
        return e
    
    l=_monosat.Not(_monosat.addAnd( ni.getLit(),_monosat.Not(nt.getLit()) ))
    r=_monosat.Not(_monosat.addAnd(_monosat.Not( ni.getLit()),_monosat.Not(ne.getLit()) ))
            
    return Var(_monosat.addAnd(l,r))

def If(condition, thn, els=true):
    return Ite(condition,thn,els)

def And(*args):

    if len(args)==0:
        return false
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            a=VAR(args[0][0])
            for i in range(1,len(args[0])):
                a=a & VAR(args[0][i])
            return a
        return VAR(args[0])
    else:
        a=VAR(args[0])
        for i in range(1,len(args)):
            a=a & VAR(args[i])
        return a;

def Or(*args):
    if len(args)==0:
        return false
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            a=VAR(args[0][0])
            for i in range(1,len(args[0])):
                a=a | VAR(args[0][i])
            return a
        return VAR(args[0])
    else:
        a=VAR(args[0])
        for i in range(1,len(args)):
            a=a| VAR(args[i])
        return a;

def Nand(*args):
    return And(args).Not()

def Nor(*args):
    return Or(args).Not()
        
def Not(a):
    return VAR(a).Not()

def Xor(*args):
    if len(args)==0:
        return false
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            a=VAR(args[0][0])
            for i in range(1,len(args[0])):
                a=a ^ VAR(args[0][i])
            return a
        return VAR(args[0])
    else:
        a=VAR(args[0])
        for i in range(1,len(args)):
            a=a ^ VAR(args[i])
        return a;
    
    #return VAR(a)^VAR(b)        
 
def Xnor(*args):
    return Xor(args).Not()
#def Xnor(a,b):
#    return VAR(a).xnor(VAR(b))

#a implies b
def Implies(a,b):
    return Not(VAR(a)) | VAR(b) #VAR(a) & Not(b)

def Eq(a,b):
    return Xnor(a,b)
    
def Neq(a,b):
    return Xor(a,b)

#Asserting versions of these, to avoid allocating extra literals...


def AssertOr(args):    
    if len(args)==0:
        Assert(false)
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            _addClause(args[0])
    else:
        _addClause(args)

def AssertNor(args):    
    if len(args)==0:
        Assert(true)
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            AssertNor(args[0])
    else:
        #AssertAnd((v.Not() for v in args))
        for v in args:
            Assert(VAR(v).Not())

def AssertAnd(args):    
    if len(args)==0:
        Assert(false)
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            AssertAnd(args[0])
    else:
        for v in args:
            Assert(v)

def AssertNand(args):    
    if len(args)==0:
        Assert(true)
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            AssertNand(args[0])
    else:
        AssertOr([VAR(v).Not() for v in args])
        
            

def AssertXor(args):
    if len(args)==0:
        return false
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            AssertXor(args[0])
    elif len(args)==2:
        _addSafeClause((VAR(args[0]),VAR(args[1])))  
        _addSafeClause((~VAR(args[0]),~VAR(args[1])))
    else:     
        #This can probably be done better...  
        a=VAR(args[0])
        for i in range(1,len(args)):
            a=a ^ VAR(args[i])
        Assert(a)

def AssertXnor(args):
    if len(args)==0:
        return true
    elif len(args)==1:
        if isinstance(args[0], collections.Iterable):
            AssertXnor(args[0])
    elif len(args)==2:
        _addSafeClause((~VAR(args[0]),VAR(args[1])))  
        _addSafeClause((VAR(args[0]),~VAR(args[1])))
    else:     
        #This can probably be done better...  
        a=VAR(args[0])
        for i in range(1,len(args)):
            a=a ^ VAR(args[i])
        Assert(~a)

#a implies b
def AssertImplies(a,b):
    _addSafeClause((~VAR(a),VAR(b)))  
    #return Not(VAR(a)) | VAR(b) #VAR(a) & Not(b)

def AssertEq(a,b):
    AssertXnor((a,b))
    
def AssertNeq(a,b):
    AssertXor((a,b))
    #return Xor(a,b)

def AssertClause(clause):
    _addClause(clause)
    #_monosat.addClause([a.getLit() for a in clause])

def Assert(a):
    
    if(a is False):
        print("Warning: asserted false literal")
    a=VAR(a)
    if(a.isConstFalse()):
        print("Warning: asserted constant false variable")
    _monosat.addUnitClause(a.getLit())

def AssertIf(If, Thn, Els=None):
    if(Els):
        AssertAnd(Or(Not(If), Thn),  Or(If, Els)  );
    else:
        AssertOr(Not(If), Thn);
        

def HalfAdd(a,b):
    aV = VAR(a)
    bV = VAR(b)

    sum = (a ^ b) 
    carry = (a &b)

    return sum,carry  

#Add these bits (or arrays of bits)
def Add(a,b,c=false):
    if(isinstance(a, collections.Iterable)):
        return _AddArray(a,b,c)
    aV = VAR(a)
    bV = VAR(b)
    cV= VAR(c)
        
    if(cV.isConstFalse()):
        sum = (a ^ b)
        carry = (a &b)
    elif cV.isConstTrue():
        sum = (a ^ b).Not()
        carry = (a | b) #Because carry is true, if either a or b, then we will output a carry
    else:    
        sum = (a ^ b) ^ c
        carry = ((a &b) | (c & (a^b)))

    return sum,carry  

#Subtract these bits (or arrays of bits) using 2's complement
def Subtract(a,b):
    if(isinstance(a, collections.Iterable)):
        return _SubtractArray(a,b)
    aV = VAR(a)
    bV = Not(VAR(b))

    sum = (a ^ b).Not()
    carry = (a | b) #Because carry is true, if either a or b, then we will output a carry

    return sum,carry  

def AddOne(array, bit=true):
    if(isFalse(bit)):
        return list(array)
    a1 = list(array)
    output = []
    sum,carry = HalfAdd(a1[0],bit)
    output.append(sum)
    for i in range(1, len(a1)):     
        sum,carry = HalfAdd(a1[i],carry)
        output.append(sum)        
    return output,carry

def _numberAsArray(number):        
    if(isinstance(number,(int, float, complex))):
        val1 = number
        n = 0
        num=[]
        while (1<<n <= val1):
            if (1<<n & val1) ==0:
                num.append(false)
            else:
                num.append(true)
            n+=1
        return num
    else:
        raise(Exception("Cannot convert to binary: " + str(number)))

def _AddArray(array1, array2,carry=false):
    if not isinstance(array1, collections.Iterable):
        array1 = _numberAsArray(array1)
    if not isinstance(array2, collections.Iterable):
        array2 = _numberAsArray(array2)
        
    a1 = list(array1)
    a2= list(array2)
    while(len(a1)<len(a2)):
        a1.append(false)
    while(len(a2)<len(a1)):
        a2.append(false)
    
    output = []
    for i in range(0, len(a1)):
        sum,carry = Add(a1[i],a2[i],carry)
        output.append(sum)
    
    output.append(carry)
    return output,carry   
    #return output+carry


def _SubtractArray(array1, array2):
    """returns array1 - array2
    """
    a1 = list(array1)
    a2= list(array2)
    while(len(a1)<len(a2)):
        a1.append(false)
    while(len(a2)<len(a1)):
        a2.append(false)
    carry = true
    output = []
    for i in range(0, len(a1)):
        sum,carry = Add(a1[i],Not(a2[i]))
        output.append(sum)
        
    return output,carry

def Min(array1, array2):
    a = list(array1)
    b= list(array2)
    while(len(a)<len(b)):
        a.append(false)
    while(len(b)<len(a)):
        b.append(false)
    
    a_less_eq = LessEq(a,b)
    output = []
    for (a_bit,b_bit) in zip(a,b):
        output.append(If(a_less_eq,a_bit,b_bit))
    return output

def Max(array1, array2):
    a = list(array1)
    b= list(array2)
    while(len(a)<len(b)):
        a.append(false)
    while(len(b)<len(a)):
        b.append(false)
    
    a_less_eq = LessEq(a,b)
    output = []
    for (a_bit,b_bit) in zip(a,b):
        output.append(If(a_less_eq,b_bit,a_bit))
    return output

#True IFF num1 is < num2, <= num2
def _LessOrEqual(num1, num2):        
    if(isinstance(num1,(int, float, complex)) and isinstance(num2,(int, float, complex))):
        return (num1<num2, num1<=num2)
    
    if(isinstance(num1,(int, float, complex))):
        num1 = _numberAsArray(num1)
    if(isinstance(num2,(int, float, complex))):
        num2 = _numberAsArray(num2)    
    
    allconst2 = True
    if(not isinstance(num2,(int, float, complex))):
        for v in num2:
            aV = VAR(v)
            if(not aV.isConst()):
                allconst2=False
                break

    if(allconst2):
        a1 = list(num1)
        if isinstance(num2,(int, float, complex)):
            val2 = num2
            n = 0
            num2=[]
            while (1<<n <= val2):
                if (1<<n & val2) ==0:
                    num2.append(false)
                else:
                    num2.append(true)
                n+=1
        else:
            val2 = 0
            for i in range(len(num2)):
                aV = VAR(num2[i])
                assert(aV.isConst())
                if(aV.isConstTrue()):
                    val2+=1<<i            
        
        if (val2> math.pow(2, len(num1))):
            return (true,true)
            
        a2= list(num2)
        while(len(a1)<len(a2)):
            a1.append(false)
        while(len(a2)<len(a1)):
            a2.append(false)
            
        lt = false
        gt = false
        for i in range(len(a1)-1,-1,-1):
            a = VAR(a1[i])
            b= VAR(a2[i])
            gt |= And(Not(lt), a, Not(b))
            lt |= And(Not(gt), Not(a),b)
        
        return lt, Or(lt, Not(gt))
    
           
    a1 = list(num1)
    a2= list(num2)
    while(len(a1)<len(a2)):
        a1.append(false)
    while(len(a2)<len(a1)):
        a2.append(false)
    lt = false
    gt = false
    for i in range(len(a1)-1,-1,-1):
        a = VAR(a1[i])
        b= VAR(a2[i])
        gt |= And(Not(lt), a, Not(b))
        lt |= And(Not(gt), Not(a),b)
    
    return lt, Or(lt, Not(gt))

def isTrue(v):
    return VAR(v).isConstTrue()

def isFalse(v):
    return VAR(v).isConstFalse()

def isConst(v):
    return VAR(v).isConst()

def LessThan(num1, num2):        
    return _LessOrEqual(num1,num2)[0]

def LessEq(num1, num2):        
    return _LessOrEqual(num1,num2)[1]

def Equal(num1,num2):
    if (isinstance(num1,(bool, int,  float, complex)) and isinstance(num2,(bool, int,  float, complex))):
        return num1==num2
    elif (isinstance(num1,Var) and isinstance(num2,Var)):
        return num1==num2
    elif (isinstance(num1, (bool, int,  float, complex))):
        return Equal(num2,num1)
    elif (isinstance(num2,(bool, int,  float, complex))):
        n=0
        t=[]
        while (1<<n <= num2):
            if (1<<n & num2) ==0:
                t.append(false)
            else:
                t.append(true)
            n+=1
        
        num2 = t
    
    
    
    a = list(num1)
    b = list(num2)
    while(len(a)<len(b)):
        a.append(false)
    while(len(b)<len(a)):
        b.append(false)
    
    allequal=true
    for i in range(len(a)):
        ai = VAR(a[i])
        bi = VAR(b[i])
        allequal &= ai.xnor(bi)
    return allequal
        

def GreaterThan(num1, num2):    
    return LessThan(num2,num1)    

def GreaterEq(num1, num2):    
    return LessEq(num2,num1)    
 

def PopCount(vars):
    if(isinstance(vars,(int, float, complex))):
        return bin(vars).count('1')

    allconst = True
    for v in vars:
        aV = VAR(v)
        if(not aV.isConst()):
            allconst=False
            break
        
    if allconst:
        count = 0    
        for v in vars:
            aV = VAR(v)
            if (v.isConstTrue()):
                count+=1
        n=0
        output=[]
        while (1<<n <= count):
            if (1<<n & count) ==0:
                output.append(false)
            else:
                output.append(true)
            n+=1
        return output

    
    maxwidth = math.ceil( math.log(len(vars),2))+2
    output=[]
    for i in range(maxwidth):
        output.append(false)
    #simple, suboptimal adder
    
    for v in vars:
        a = VAR(v)
        if(not a.isConstFalse()):           
            output,carry = Add(output,[a])

                
         
    return output
    
    
    
    while(len(vars)>1):
        next=[]
        next_carry=[]
        for i in range(0,len(vars),3):
            assert(i<len(vars))
            a=vars[i]
            b=false
            c=false
            if(i+1<len(vars)):
                b= vars[i+1]
            if(i+2<len(vars)):
                c= vars[i+2]        
            sum,carry = Add(a,b,c);
            next.append(sum)
            next.append(carry)
        vars=next
    

#Returns true if exactly one variable in the array is true.
#@functools.lru_cache(None)
def OneHot(*arrayVars):
    sum,carry = false,false
    eversum = false
    evercarry=false
    for x in arrayVars:
        sum,carry = Add(sum,x,carry)
        eversum = eversum | sum
        evercarry = evercarry | carry
        
    return eversum & ~evercarry

def PopLE( constant,*arrayVars):
    return PopLT(constant+1,*arrayVars)
#True if the population count of the vars in the array is exactly equal to the (integer) constant
#Note: this is an UNSIGNED comparison


def PopEq( compareTo,*arrayVars):
    if len(arrayVars)==1 and isinstance(arrayVars[0], collections.Iterable):
        arrayVars=arrayVars[0]
    #build up a tree of add operations
    if (isinstance(compareTo,(bool, int,  float, complex))):
        if(compareTo<0):
            print("Warning: Passed negative constant to popcount methods, which expect unsigned arguments")
            return false;#shouldn't really come up... 
        elif (compareTo==0):
            return Not(Or(*arrayVars))    
        elif(compareTo==1):
            return OneHot(*arrayVars)
        elif compareTo==len(arrayVars):
            return And(*arrayVars)
        elif compareTo>len(arrayVars):
            return false

    count = PopCount(arrayVars)  
    return Equal(count,compareTo)  

#Note: this is an UNSIGNED comparison
def PopLT( compareTo,*arrayVars):
    if len(arrayVars)==1 and isinstance(arrayVars[0], collections.Iterable):
        arrayVars=arrayVars[0]
    #build up a tree of add operations
    if (isinstance(compareTo,(bool, int, float, complex))):
        if(compareTo<0):
            print("Warning: Passed negative constant to popcount methods, which expect unsigned arguments")
            return false;#shouldn't really come up... 
        elif (compareTo==0):
            return false    
        elif(compareTo==1):
            return Not(Or(arrayVars))    
        elif (compareTo==2):
            return OneHot(*arrayVars)
        elif compareTo==len(arrayVars):
            return Not(And(arrayVars))
        elif compareTo>len(arrayVars):
            return true
    count = PopCount(arrayVars)  
    return LessThan(count,compareTo)  

#Note: this is an UNSIGNED comparison
def PopGE( constant,*arrayVars):
    return PopGT(constant-1,*arrayVars)

#True if the population count of the vars in the array is exactly equal to the (integer) constant
#Note: this is an UNSIGNED comparison
def PopGT( constant,*arrayVars):
    return Not(PopLE(constant,*arrayVars))   




