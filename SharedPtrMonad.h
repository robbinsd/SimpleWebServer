#pragma once

#include <memory>
#include <functional>
#include "Curry.h"

// fmap :: (a -> b) -> f a -> f b
// AKA lift
template <typename A, typename R>
std::shared_ptr<R> FunctorTransform(std::function<R(A)> func, std::shared_ptr<A> value)
{
    if (value)
    {
        return std::make_shared<R>(func(*value));
    }

    return std::shared_ptr<R>();
}
template <typename A, typename B, typename R>
std::shared_ptr<R> FunctorTransform2(std::function<R(A, B)> func, std::shared_ptr<A> aVal, std::shared_ptr<B> bVal)
{
    return FunctorApply(FunctorTransform(curry(func), aVal), bVal);
}
template <typename A, typename B, typename C, typename R>
std::shared_ptr<R> FunctorTransform3(std::function<R(A, B, C)> func, std::shared_ptr<A> aVal, std::shared_ptr<B> bVal, std::shared_ptr<C> cVal)
{
    return FunctorApply(FunctorApply(FunctorTransform(curry(func), aVal), bVal), cVal);
}

// apply :: f (a -> b) -> f a -> f b
template <typename A, typename R>
std::shared_ptr<R> FunctorApply(std::shared_ptr< std::function<R(A)> > func, std::shared_ptr<A> value)
{
    // TODO: how is this used again?
    // Used to iteratively apply function of N arguments to N functor values.
    if (func)
    {
        return FunctorTransform(*func, value);
    }

    return std::shared_ptr<R>();
}

// bind :: m a -> (a -> m b) -> m b
template <typename A, typename B>
std::shared_ptr<B> MonadBind(std::shared_ptr<A> value, std::function<std::shared_ptr<B>(A)> func)
{
    if (value)
    {
        return func(*value);
    }

    return std::shared_ptr<B>();
}
template <typename A, typename B>
std::shared_ptr<B> operator>=(std::shared_ptr<A> value, std::function<std::shared_ptr<B>(A)> func)
{
    return MonadBind(value, func);
}

// TODO: experiment with using CurriedFunc everywhere. Look up if it's already part of language.
/*template <typename R, typename A, typename B>
class CurriedFunc
{
public:
    CurriedFunc(std::function<R(A,B)> func) : m_func(func) {}

    std::function<R(B)> operator() (A a)
    {
        return std::bind(m_func, a);
    }

    R operator() (A a, B b)
    {
        return m_func(a, b);
    }

protected:
    std::function<R(A,B)> m_func;
};*/