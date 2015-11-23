#pragma once

#include <string>

#include <boost/variant.hpp>

using namespace std;

class sqlite3;

class Database
{
public:
	struct Error
	{
		Error() : m_code(0) { }
		Error(string message) : m_message(message), m_code(1) { }

		string m_message;
		int m_code;
	};
	typedef string Name;
	template<typename T> using Failable = boost::variant<Error, T>;

	static Failable<Database> Create();

	Failable<Name> GetName(string id); // output: errorMsg
	Error ReplaceName(string id, Name name); //output: errorMsg

	/* TODO: experiment with boost::variant.
	 apply_visitor is like "lift" (or fmap). It lifts an operation into the variant monad.
	 variant is C++ equivalent of haskell Either. Research how Either behaves.
	*/
protected:
	Database(sqlite3* dbPtr) : m_dbPtr(dbPtr) { }
	sqlite3* m_dbPtr;
};