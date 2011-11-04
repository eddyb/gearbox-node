// Copyright (c) 2011 the gearbox-node project authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef GEARBOX_VALUE_H
#define GEARBOX_VALUE_H

#include <gearbox.h>

#include <iostream>

namespace Gearbox {
    
    class Primitive {
        public:
            enum Kind {
                Undefined=0,
                Null,
                False,
                True,
                Integer,
                Number
            };
            Primitive(Kind kind=Undefined, int64_t iValue=0) : m_Kind(kind) {
                if(kind == Integer)
                    m_iValue = iValue;
                else if(kind == Number)
                    m_dValue = iValue;
            }
            Primitive(Kind kind, double dValue) : m_Kind(kind) {
                if(kind == Integer)
                    m_iValue = dValue;
                else if(kind == Number)
                    m_dValue = dValue;
            }
            
            operator v8::Handle<v8::Value>() const {
                if(m_Kind == Undefined)
                    return v8::Undefined();
                else if(m_Kind == Null)
                    return v8::Null();
                else if(m_Kind == False)
                    return v8::False();
                else if(m_Kind == True)
                    return v8::True();
                else if(m_Kind == Integer) {
                    if(m_iValue >= 0 && m_iValue <= 0xffffffffL)
                        return v8::Integer::NewFromUnsigned(m_iValue);
                    else if(m_iValue >= -0x80000000L && m_iValue <= 0x7fffffffL)
                        return v8::Integer::New(m_iValue);
                    else
                        return v8::Number::New(m_iValue);
                }
                else if(m_Kind == Number)
                    return v8::Number::New(m_dValue);
                return v8::Undefined();
            }
            
            void from(v8::Handle<v8::Value> hValue) {
                if(hValue->IsNumber()) {
                    m_Kind = Number;
                    m_dValue = hValue->NumberValue();
                }
                else if(hValue->IsUint32() || hValue->IsInt32()) {
                    m_Kind = Integer;
                    m_iValue = hValue->IntegerValue();
                }
                else if(hValue->IsNull())
                    m_Kind = Null;
                else if(hValue->IsFalse())
                    m_Kind = False;
                else if(hValue->IsTrue())
                    m_Kind = True;
                else 
                    m_Kind = Undefined;
            }
            
            bool operator==(Primitive that) {
                if(m_Kind <= Null)
                    return m_Kind == that.m_Kind;
                return operator v8::Handle<v8::Value>()->Equals(that);
            }
            
            bool operator!=(Primitive that) {
                return !operator==(that);
            }
            
        private:
            Kind m_Kind;
            union {
                int64_t m_iValue;
                double m_dValue;
            };
            friend class Value;
    };
    
    static Primitive undefined;
    static Primitive null(Primitive::Null);
    
    template <class Node, class Index>
    class Assignable : public Node {
        public:
            Assignable(const Node &parent, const Index &index)/* : m_Parent(parent), m_Index(index)*/ {
                m_Parent = parent;
                m_Index = index;
                from(m_Parent.get(m_Index));
            }
            template <class T>
            Node &operator=(const T &_that) {
                Node that(_that);
                m_Parent.set(m_Index, that);
                return *this;
            }
            template <class T>
            Node &operator=(const T &&_that) {
                Node that(_that);
                m_Parent.set(m_Index, that);
                return *this;
            }
            Node &operator=(const Node &that) {
                m_Parent.set(m_Index, that);
                return *this;
            }
            Node &operator=(const Node &&that) {
                m_Parent.set(m_Index, that);
                return *this;
            }
            template <class... Args>
            Node operator()(Args... _args) {
                return call(m_Parent, _args...);
            }
            
        private:
            Node m_Parent;
            Index m_Index;
    };
    
    /** A class for every kind of JavaScript value (Objects, Arrays, Functions, Numbers, Strings) */
    class Value {
        public:
            /** Default constructor */
            Value() {}
            /** Constructors */
            template <class T>
            Value(T that) {
                from(that);
            }
            /** Default destructor */
            virtual ~Value() {
                if(!m_hValue.IsEmpty())
                    m_hValue.MakeWeak(0, weakCallback);
            }
            
            /** Copy operators */
            template <class T>
            Value &operator=(T that) {
                if(!m_hValue.IsEmpty()) {
                    m_hValue.MakeWeak(0, weakCallback);
                    m_hValue.Clear();
                }
                from(that);
                return *this;
            }
            
            /** Instantiation tools */
            void from(const Value &that) {
                if(that.m_hValue.IsEmpty())
                    from(that.m_pValue);
                else
                    from(that.m_hValue);
            }
            void from(v8::Handle<v8::Value>);
            template <class T>
            void from(v8::Handle<T> that) {
                from(v8::Handle<v8::Value>(that));
            }
            void from(String that) {
                from(that.operator v8::Handle<v8::Value>());
            }
            void from(Primitive);
            void from(const char *that) {
                from(v8::String::New(that));
            }
            void from(char *that) {
                from(v8::String::New(that));
            }
#ifdef __LP64__
            void from(unsigned long long int that) {
                from(Primitive(Primitive::Integer, int64_t(that)));
            }
            void from(long long int that) {
                from(Primitive(Primitive::Integer, int64_t(that)));
            }
#endif
            void from(uint64_t that) {
                from(Primitive(Primitive::Integer, int64_t(that)));
            }
            void from(int64_t that) {
                from(Primitive(Primitive::Integer, that));
            }
            void from(int that) {
                from(int64_t(that));
            }
            void from(unsigned int that) {
                from(int64_t(that));
            }
            void from(double that) {
                from(Primitive(Primitive::Number, that));
            }
            void from(bool that) {
                from(Primitive(that ? Primitive::True : Primitive::False));
            }
            void from(void *that) {
                from(v8::External::New(that));
            }
            
            /** Conversion tools, used to get primitive values */
            template <class T>
            T to() const {
                return to(Type<T>());
            }
            
            v8::Handle<v8::Value> to(Type<v8::Handle<v8::Value>>) const;
            v8::Handle<v8::Data> to(Type<v8::Handle<v8::Data>>) const {
                return to<v8::Handle<v8::Value>>();
            }
            template <class T>
            v8::Handle<T> to(Type<v8::Handle<T>>) const {
                return v8::Handle<T>::Cast(to<v8::Handle<v8::Value>>());
            }
            Primitive to(Type<Primitive>) const {
                if(m_hValue.IsEmpty())
                    return m_pValue;
                Primitive value;
                value.from(m_hValue);
                return value;
            }
            String to(Type<String>) const;
            int64_t to(Type<int64_t>) const;
#ifdef __LP64__
            int64_t to(Type<long long int>) const {
                return to<int64_t>();
            }
            uint64_t to(Type<unsigned long long int>) const {
                return to<int64_t>();
            }
#endif
            uint64_t to(Type<uint64_t>) const {
                return to<int64_t>();
            }
            uint32_t to(Type<uint32_t>) const {
                return to<int64_t>();
            }
            uint16_t to(Type<uint16_t>) const {
                return to<int64_t>();
            }
            int16_t to(Type<int16_t>) const {
                return to<int64_t>();
            }
            char to(Type<char>) const {
                return to<int64_t>();
            }
            uint8_t to(Type<uint8_t>) const {
                return to<int64_t>();
            }
            int8_t to(Type<int8_t>) const {
                return to<int64_t>();
            }
            int to(Type<int>) const {
                return to<int64_t>();
            }
            double to(Type<double>) const;
            float to(Type<float>) const {
                return to<double>();
            }
            long double to(Type<long double>) const {
                return to<double>();
            }
            bool to(Type<bool>) const;
            
            template <class T>
            T *to(Type<T*>) const {
                std::cerr << "Value::to<T*> is deprecated!" << std::endl;
                if(m_hValue.IsEmpty() || !m_hValue->IsExternal() || !v8::External::Unwrap(m_hValue)) {
                    std::cerr << "Empty/NULL External!" << std::endl;
                    return 0;
                }
                return reinterpret_cast<T*>(v8::External::Unwrap(m_hValue));
            }
            
            /** Compare operators */
#define DECLARE_OP(OP) \
bool operator OP(Primitive that) { \
    if(m_hValue.IsEmpty()) \
        return m_pValue OP that; \
    else \
        return m_hValue->Equals(that);\
}
            bool operator==(Value that) {
                if(m_hValue.IsEmpty() && that.m_hValue.IsEmpty())
                    return m_pValue == that.m_pValue;
                else
                    return to<v8::Handle<v8::Value>>()->Equals(that);
            }
            bool operator!=(Value that) {
                return !operator==(that);
            }
            //DECLARE_OP(==)
            //DECLARE_OP(!=)
            //DECLARE_OP(>)
            //DECLARE_OP(<)
            //DECLARE_OP(>=)
            //DECLARE_OP(<=)
#undef DECLARE_OP
            
            /** Length, for Arrays and Strings */
            int length() const;
            
            /** Access to Object or Array elements */
            Assignable<Value, uint32_t> operator[](uint32_t idx) const {
                return Assignable<Value, uint32_t>(*this, idx);
            }
            Assignable<Value, uint32_t> operator[](int32_t idx) const {
                return Assignable<Value, uint32_t>(*this, idx);
            }
            Assignable<Value, uint32_t> operator[](size_t idx) const {
                return Assignable<Value, uint32_t>(*this, idx);
            }
            Assignable<Value, v8::Handle<v8::String>> operator[](const char *idx) const {
                return Assignable<Value, v8::Handle<v8::String>>(*this, v8::String::NewSymbol(idx));
            }
            /*Assignable<Value, String> operator[](const String &idx) const {
                return Assignable<Value, String>(*this, idx);
            }*/
            Value get(uint32_t idx) const {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return undefined;
                return m_hValue->ToObject()->Get(idx);
            }
            Value get(String idx) const {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return undefined;
                return m_hValue->ToObject()->Get(idx.operator v8::Handle<v8::Value>());
            }
            Value get(v8::Handle<v8::String> idx) const {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return undefined;
                return m_hValue->ToObject()->Get(idx);
            }
            void set(uint32_t idx, const Value &val) {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return;
                m_hValue->ToObject()->Set(idx, val);
            }
            void set(String idx, const Value &val) {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return;
                m_hValue->ToObject()->Set(idx.operator v8::Handle<v8::Value>(), val);
            }
            void set(v8::Handle<v8::String> idx, const Value &val) {
                if(m_hValue.IsEmpty() || !m_hValue->IsObject())
                    return;
                m_hValue->ToObject()->Set(idx, val);
            }
            
            /** Returns true if this Value is an instance of class T */
            template <class T>
            bool is() const {
                return T::is(*this);
            }
            
            /** Convert operator */
            template <class T>
            operator T() const {
                return to<T>();
            }
            
            /** Call operator for Functions */
            template <class... Args>
            Value operator()(Args... _args) const {
                return call(v8::Context::GetCurrent()->Global(), _args...);
            }
            
            template <class... Args>
            Value call(Value _this, Args... _args) const {
                if(m_hValue.IsEmpty() || !m_hValue->IsFunction())
                    return undefined;
                
                v8::Handle<v8::Value> args[sizeof...(_args)];
                placeArgs(args, _args...);
                
                // Exceptions can be thrown, we are inside JavaScript
                bool bCanThrowBefore = tryCatchCanThrow(true);
                
                Value result = v8::Handle<v8::Function>::Cast(m_hValue)->Call(_this, sizeof...(_args), args);
                
                // We are back from JavaScript
                tryCatchCanThrow(bCanThrowBefore);
                
                return result;
            }
            
            /** New Instance for Functions */
            template <class... Args>
            Value newInstance(Args... _args) const {
                if(m_hValue.IsEmpty() || !m_hValue->IsFunction())
                    return undefined;
                
                v8::Handle<v8::Value> args[sizeof...(_args)];
                placeArgs(args, _args...);
                
                // Exceptions can be thrown, we are inside JavaScript
                bool bCanThrowBefore = tryCatchCanThrow(true);
                
                Value result = v8::Handle<v8::Function>::Cast(m_hValue)->NewInstance(sizeof...(_args), args);
                
                // We are back from JavaScript
                tryCatchCanThrow(bCanThrowBefore);
                
                return result;
            }
            
        private:
            template <class... Last>
            static void placeArgs(v8::Handle<v8::Value> *pValues, Value first, Last... last) {
                *pValues = first;
                placeArgs(pValues + 1, last...);
            }
            
            static void placeArgs(v8::Handle<v8::Value> *pValues) {}
            
            /// FIXME Hack to get TryCatch::canThrow into call and newInstance.
            static bool tryCatchCanThrow(bool);
            
            static void weakCallback(v8::Persistent<v8::Value>, void*);
            
            Primitive m_pValue;
            v8::Persistent<v8::Value> m_hValue;
            
            friend class Assignable<Value, uint32_t>;
            friend class Assignable<Value, String>;
    };
}

#include "TryCatch.h"

namespace Gearbox {
    inline bool Value::tryCatchCanThrow(bool bCanThrow) {
        return TryCatch::canThrow(bCanThrow);
    }
}

#endif
