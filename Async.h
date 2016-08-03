#pragma once

#include <memory>
#include <functional>
#include "Curry.h"

template <typename A> 
using Callback = std::function<void(A)>;

template <typename A>
class Async
{
public:
    typedef std::function<void(Callback<A>)> FuncType;

    Async(FuncType func) : m_func(func) { }

    template <typename B>
    Async<B> Then(std::function<B(A)> func) const
    {
        return FunctorTransform(func, *this);
    }

    template <typename B>
    Async<B> Then(std::function<Async<B>(A)> func) const
    {
        return MonadBind(*this, func);
    }

    void BeginExecution(Callback<A> continuation) const
    {
        m_func(continuation);
    }

private:
    const FuncType m_func;
};

// pure :: a -> f a
// AKA return
template <typename A>
Async<A> MakeAsync(A value)
{
    return [=](Callback<A> cont)
    {
        cont(value);
    };
}
// (() -> a) -> f a
template <typename A>
Async<A> MakeFuncAsync(std::function<A()> func)
{
    return [=](Callback<A> cont)
    {
        cont(func());
    };
}

template <typename T>
using Lazy = std::function<T()>;

// fmap :: (a -> b) -> f a -> f b
// AKA lift
template <typename A, typename R>
Async<R> FunctorTransform(std::function<R(A)> func, Async<A> asyncA)
{
    return [=](Callback<R> contR)
    {
        Callback<A> contA = [=](A a) {
            contR(func(a));
        };
        asyncA.BeginExecution(contA);
    };
}
template <typename A, typename R>
Async<R> FunctorTransformm(Async<A> asyncA, std::function<R(A)> func)
{
    return [=](Callback<R> contR)
    {
        Callback<A> contA = [=](A a) {
            contR(func(a));
        };
        asyncA.BeginExecution(contA);
    };
}
template <typename A, typename B, typename R>
Async<R> FunctorTransform2(std::function<R(A, B)> func, Async<A> aVal, Async<B> bVal)
{
    return FunctorApply(FunctorTransform(curry(func), aVal), bVal);
}
template <typename A, typename B, typename C, typename R>
Async<R> FunctorTransform3(std::function<R(A, B, C)> func, Async<A> aVal, Async<B> bVal, Async<C> cVal)
{
    return FunctorApply(FunctorApply(FunctorTransform(curry(func), aVal), bVal), cVal);
}

// apply :: f (a -> b) -> f a -> f b
template <typename A, typename R>
Async<R> FunctorApply(Async< std::function<R(A)> > asyncFuncAtoR, Async<A> asyncA)
{
    return [=](Callback<R> contR)
    {
        Callback<std::function<R(A)>> contFuncAtoR = [=](std::function<R(A)> funcAtoR)
        {
            Async<R> asyncR = FunctorTransform(funcAtoR, asyncA);
            asyncR(contR);
        };
        asyncFuncAtoR(contFuncAtoR);
    };
}

// bind :: m a -> (a -> m b) -> m b
template <typename A, typename B>
Async<B> MonadBind(Async<A> value, std::function<Async<B>(A)> func)
{
    return [=](Callback<B> contB)
    {
        Callback<A> contA = [=](A a) {
            Async<B> proxyAsyncB = func(a);
            proxyAsyncB(contB);
        };
        value(contA);
    };
}
template <typename A, typename B>
Async<B> operator>=(Async<A> value, std::function<Async<B>(A)> func)
{
    return MonadBind(value, func);
}
