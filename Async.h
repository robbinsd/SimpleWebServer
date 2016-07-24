#pragma once

#include <memory>
#include <functional>
#include "Curry.h"

template <typename A> 
using Continuation = std::function<void(A)>;

template <typename A>
using Async = std::function<void(Continuation<A>)>;

// pure :: a -> f a
// AKA return
template <typename A>
Async<A> MakeAsync(A value)
{
    return [=](Continuation<A> cont)
    {
        cont(value);
    };
}

// fmap :: (a -> b) -> f a -> f b
// AKA lift
template <typename A, typename R>
Async<R> FunctorTransform(std::function<R(A)> func, Async<A> asyncA)
{
    return [=](Continuation<R> contR)
    {
        Continuation<A> contA = [=](A a) {
            contR(func(a));
        };
        asyncA(contA);
    };
}
template <typename A, typename R>
Async<R> FunctorTransformm(Async<A> asyncA, std::function<R(A)> func)
{
    return [=](Continuation<R> contR)
    {
        Continuation<A> contA = [=](A a) {
            contR(func(a));
        };
        asyncA(contA);
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
    return [=](Continuation<R> contR)
    {
        Continuation<std::function<R(A)>> contFuncAtoR = [=](std::function<R(A)> funcAtoR)
        {
            Async<R> asyncR = FunctorTransform(funcAtoR, asyncA);
            asyncR(contR);
        };
        asyncFuncAtoR(contFuncAtoR);
    };
}

// bind :: m a -> (a -> m b) -> m b
template <typename A, typename B>
Async<B> FunctorBind(Async<A> value, std::function<Async<B>(A)> func)
{
    return [=](Continuation<B> contB)
    {
        Continuation<A> contA = [=](A a) {
            Async<B> proxyAsyncB = func(a);
            proxyAsyncB(contB);
        };
        value(contA);
    };
}
template <typename A, typename B>
Async<B> operator>=(Async<A> value, std::function<Async<B>(A)> func)
{
    return FunctorBind(value, func);
}
