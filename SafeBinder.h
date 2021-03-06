#pragma once

#include "FunctionUtils.h"
#include "FunctionTraits.h"

#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>

namespace QtSignalTools
{

// StrongRef is a helper class which takes
// a weak reference to an object and promotes
// it to a strong reference for the lifetime of the StrongRef instance,
// providing access to the underlying object via a data() method.
//
template <class T>
struct StrongRef;

// version for QWeakPointer<T>
template <class T>
struct StrongRef<QWeakPointer<T> >
{
	StrongRef(QWeakPointer<T>& ref)
		: m_strongRef(ref.toStrongRef())
		, m_weakRef(ref)
	{}

	T* data() const {
		if (m_strongRef) {
			return m_strongRef.data();
		} else {
			// this may either be a QWeakPointer constructed
			// from a QObject* which is not tracked by a QSharedPointer or
			// the object may have been deleted, in which case data() will
			// return 0
			return m_weakRef.data();
		}
	}

	QSharedPointer<T> m_strongRef;
	QWeakPointer<T> m_weakRef;
};

// version for QPointer<T>.
template <class T>
struct StrongRef<QPointer<T> >
{
	StrongRef(QPointer<T>& ref)
		: m_ref(ref)
	{
		// There is no way to actually promote QPointer<T> to
		// a strong reference, so we can't do that here as
		// we do for other StrongRef implementations
	}

	T* data() const {
		return m_ref.data();
	}

	QPointer<T> m_ref;
};

// version for weak_ptr<T>
template <template <class T> class WeakPointer, class T>
struct StrongRef<WeakPointer<T> >
{
	StrongRef(WeakPointer<T>& ref)
		: m_strongRef(ref.lock())
	{}

	T* data() const {
		return m_strongRef.get();
	}

	shared_ptr<T> m_strongRef;
};

template <class Receiver, class MemberFunc>
class SafeBinder
{
	public:
		typedef typename MemberFuncResultType<MemberFunc>::type result_type;

		SafeBinder(Receiver receiver, MemberFunc func)
		: m_receiver(receiver)
		, m_func(func)
		{}

#define SAFE_BINDER_CALL_OP(typesExpr, paramExpr, argsExpr) \
		typesExpr\
		result_type operator()(paramExpr) {\
			StrongRef<Receiver> strongRef(m_receiver);\
			if (strongRef.data()) {\
				return (strongRef.data()->*m_func)(argsExpr);\
			} else {\
				return result_type();\
			}\
		}

#ifdef QST_COMPILER_SUPPORTS_VARIADIC_TEMPLATES
		SAFE_BINDER_CALL_OP(template <typename... Args>, Args... args, args...)
#else
		SAFE_BINDER_CALL_OP(,,)
		SAFE_BINDER_CALL_OP(template <class T1>, const T1& arg1, arg1)
		SAFE_BINDER_CALL_OP(template <class T1 QST_COMMA class T2>,
							const T1& arg1 QST_COMMA const T2& arg2,
							arg1 QST_COMMA arg2);
		SAFE_BINDER_CALL_OP(template <class T1 QST_COMMA class T2 QST_COMMA class T3>,
							const T1& arg1 QST_COMMA const T2& arg2 QST_COMMA const T3& arg3,
							arg1 QST_COMMA arg2 QST_COMMA arg3);
#endif

	private:
		Receiver m_receiver;
		MemberFunc m_func;
};

/** safe_bind() is a wrapper around a method call which does
 * nothing and returns a default value if the object is destroyed
 * before the call is invoked.
 *
 * The syntax is:
 *   safe_bind(object, method)
 *
 * Where 'object' is either a QObject, a QWeakPointer<T> or a weak_ptr<T>
 * that can be used to detect when the object is destroyed.
 *
 * Usage with QObject:
 *
 *   QObject* myObject = new QObject;
 *   myObject->setObjectName("myObject");
 *   function<QString()> getName(safe_bind(myObject, &QObject::objectName));
 *   qDebug() << getName(); // prints "myObject"
 *   delete myObject;
 *   qDebug() << getName(); // prints ""
 *
 * Usage with a non-QObject class:
 *
 *   shared_ptr<Object> myObject(new Object);
 *   myObject->setSomeProperty("foo");
 *   function<QString()> getProperty(safe_bind(weak_ptr<Object>(myObject), &Object::someProperty));
 *   qDebug() << getProperty(); // prints "foo"
 *   myObject.reset();
 *   qDebug() << getProperty(); // prints ""
 */
template <template <class T> class WeakPointer, class MemberFunc, class T>
SafeBinder<WeakPointer<T>,MemberFunc> safe_bind(const WeakPointer<T>& r, MemberFunc f)
{
	return SafeBinder<WeakPointer<T>,MemberFunc>(r,f);
}

// Under Qt 4 QWeakPointer is the most efficient weak reference
// to QObject.  Under Qt 5, QWeakPointer no longer integrates with
// QObject but QPointer was instead re-written to be more efficient
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
template <class T, class MemberFunc>
SafeBinder<QPointer<T>,MemberFunc> safe_bind(T* r, MemberFunc f,
  typename enable_if<is_base_of<QObject,T>::value,T>::type* = 0)
{
	return SafeBinder<QPointer<T>,MemberFunc>(r,f);
}
#else
template <class T, class MemberFunc>
SafeBinder<QWeakPointer<T>,MemberFunc> safe_bind(T* r, MemberFunc f,
  typename enable_if<is_base_of<QObject,T>::value,T>::type* = 0)
{
	return SafeBinder<QWeakPointer<T>,MemberFunc>(r,f);
}
#endif

}

