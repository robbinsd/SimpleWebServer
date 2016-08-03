#pragma once

#include <functional>
#include <boost/variant.hpp>

template <typename A, typename B, typename C = void> class Choice;

template <typename A, typename B>
class Choice<A, B>
{
public:
    template <typename T> Choice(T t) : m_variant(t)
    {
    }

    template <typename R>
    R SwitchCaseConst(std::function<R(const A&)> handlerA, std::function<R(const B&)> handlerB) const
    {
        return m_variant.apply_visitor(SwitchCaseConstVisitor<R>(handlerA, handlerB));
    }

    template <typename R>
    R SwitchCase(std::function<R(A&)> handlerA, std::function<R(B&)> handlerB)
    {
        return m_variant.apply_visitor(SwitchCaseVisitor<R>(handlerA, handlerB));
    }

    /*template <typename R, typename T1, typename T2>
    class SwitchCaseVisitor : public boost::static_visitor<R>
    {
    public:
        typedef std::function<R(T1&)> Type1Handler;
        typedef std::function<R(T2&)> Type2Handler;
        SwitchCaseVisitor(Type1Handler handler1, Type2Handler handler2)
            : m_handler1(handler1)
            , m_handler2(handler2)
        {
        }

        R operator()(T1 type1Value) const
        {
            return m_handler1(type1Value);
        }

        R operator()(T2 type2Value) const
        {
            return m_handler2(type2Value);
        }

    protected:
        Type1Handler m_handler1;
        Type2Handler m_handler2;
    };

    template <typename R, typename T1, typename T2>
    R SwitchCase(const boost::variant<T1, T2>& choice, std::function<R(T1&)> handler1, std::function<R(T2&)> handler2)
    {
        return choice.apply_visitor(SwitchCaseVisitor<R, T1, T2>(handler1, handler2));
    }*/

    boost::variant<A, B> GetVariant() const { return m_variant; }

protected:
    template <typename R>
    class SwitchCaseConstVisitor : public boost::static_visitor<R>
    {
    public:
        typedef std::function<R(const A&)> TypeAHandler;
        typedef std::function<R(const B&)> TypeBHandler;
        SwitchCaseConstVisitor(TypeAHandler handlerA, TypeBHandler handlerB)
            : m_handlerA(handlerA)
            , m_handlerB(handlerB)
        {
        }

        R operator()(const A& typeAValue) const
        {
            return m_handlerA(typeAValue);
        }

        R operator()(const B& typeBValue) const
        {
            return m_handlerB(typeBValue);
        }

    protected:
        TypeAHandler m_handlerA;
        TypeBHandler m_handlerB;
    };

    template <typename R>
    class SwitchCaseVisitor : public boost::static_visitor<R>
    {
    public:
        typedef std::function<R(A&)> TypeAHandler;
        typedef std::function<R(B&)> TypeBHandler;
        SwitchCaseVisitor(TypeAHandler handlerA, TypeBHandler handlerB)
            : m_handlerA(handlerA)
            , m_handlerB(handlerB)
        {
        }

        R operator()(A& typeAValue)
        {
            return m_handlerA(typeAValue);
        }

        R operator()(B& typeBValue)
        {
            return m_handlerB(typeBValue);
        }

    protected:
        TypeAHandler m_handlerA;
        TypeBHandler m_handlerB;
    };

    //friend std::ostream operator<<(std::ostream&, Choice);

    boost::variant<A, B> m_variant;
};

//template <typename A, typename B>
//std::ostream operator<<(std::ostream& out, const Choice<A,B>& choice)
//{
//    return out << choice.m_variant;
//}

struct Error
{
    Error() : m_code(0) { }
    explicit Error(std::string message) : m_message(message), m_code(1) { }

    std::string m_message;
    int m_code;
};
std::ostream& operator<< (std::ostream& out, const Error& err);

template <typename T>
class Failable : public Choice<Error, T>
{
public:
    using Choice<Error, T>::Choice;

    static Failable<T> Success(const T& value) { return Failable(value); }
    static Failable<T> Failure(const Error& error) { return Failable(error); }

    bool IsFailure() const { return m_variant.which() == 0; }
    bool IsSuccess() const { return m_variant.which() == 1; }

    Error   GetFailure() const { return boost::get<Error>(m_variant);   }
    T       GetSuccess() const { return boost::get<T>(m_variant);       }
};
template <typename A, typename R>
Failable<R> FunctorTransformm(Failable<A> failableA, std::function<R(A)> func)
{
    if (failableA.IsFailure())
    {
        return failableA.GetFailure();
    }

    return func(failableA.GetSuccess()); //implicitly construct success
}
// bind :: m a -> (a -> m b) -> m b
template <typename A, typename B>
Failable<B> MonadBind(Failable<A> failableA, std::function<Failable<B>(A)> func)
{
    if (failableA.IsFailure())
    {
        return failableA.GetFailure();
    }
    return func(failableA.GetSuccess());
}
template <typename A, typename B>
Failable<B> operator>=(Failable<A> failableA, std::function<Failable<B>(A)> func)
{
    return MonadBind(failableA, func);
}

template <typename A, typename B, typename C>
class Choice
{
public:
    template <typename T> Choice(T t) : m_variant(t)
    {
    }


protected:
    boost::variant<A, B, C> m_variant;
};