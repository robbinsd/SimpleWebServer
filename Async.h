#pragma once

#include <memory>
#include <functional>
#include "Curry.h"

template <typename A> 
using Continuation = std::function<void(A)>;

template <typename A>
using Async = std::function<void(Continuation<A>)>;

// fmap :: (a -> b) -> f a -> f b
// AKA lift
template <typename A, typename R>
Async<R> FunctorTransform(std::function<R(A)> func, Async<A> asyncA)
{
	return [=](Continuation<R> contR)
	{
		Continuation<A> contA = [=](A a) {
			contR(func(a));
		}
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
/*template <typename A, typename R>
Async<R> FunctorApply(Async< std::function<R(A)> > asyncFuncAtoR, Async<A> asyncA)
{
	Continuation<A> contA = [=](A a) {
		contR(func(a));
	}
	asyncA(contA)
	asyncFuncAtoB(contAToB)
	return [=](Continuation<R> contR)
	{
		...
			contB()
	};

	if (func)
	{
		return FunctorTransform(*func, value);
	}

	return Async<B>();
}*/

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