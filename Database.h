#pragma once

#include <string>

#include <boost/variant.hpp>

#include "Async.h"
#include "Choice.h"

using namespace std;

class sqlite3;

class Database
{
public:

    static Failable<Database> Create();

    typedef string Name;
    //template <typename T> typedef Failable<T>;

    Async<Failable<Name>> GetName(string id); // output: errorMsg
    Async<Error> ReplaceName(string id, Name name); //output: errorMsg

    /* TODO: experiment with boost::variant.
     apply_visitor is like "lift" (or fmap). It lifts an operation into the variant monad.
     variant is C++ equivalent of haskell Either. Research how Either behaves.
    */
protected:
    Database(sqlite3* dbPtr) : m_dbPtr(dbPtr) { }
    sqlite3* m_dbPtr;
};