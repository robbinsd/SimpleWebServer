#include "Database.h"

#include <sstream>
#include <iostream>
#include <vector>
#include <thread>

#include "include/sqlite/sqlite3.h"

#include "Choice.h"
#include "MakeFunction.h"

Failable<Database> Database::Create()
{
    sqlite3* dbPtr = NULL;
    sqlite3_open("test.db", &dbPtr);
    if (dbPtr == NULL)
    {
        return Error("somethign went wong");
    }

    char* errorMsg = NULL;
    sqlite3_exec(
        dbPtr,
        "CREATE TABLE IF NOT EXISTS names("
        "  id SMALLINT PRIMARY KEY,"
        "  name VARCHAR(32)"
        ");",
        NULL,
        NULL,
        &errorMsg);

    if (errorMsg)
    {
        return Error(errorMsg);
    }
    
    return Database(dbPtr);
}

Async<Error> Database::ReplaceName(string id, Database::Name name)
{
    return [=](Continuation<Error> callback)
    {
        std::thread t([=]()
        {
            stringstream stream;
            stream << "INSERT OR REPLACE INTO names (id, name)"
                "  VALUES (" << id << ", \"" << name << "\");";
            string query = stream.str();

            char* errorMsg = NULL;
            sqlite3_exec(
                m_dbPtr,
                query.c_str(),
                NULL,
                NULL,
                &errorMsg);

            Error error;
            if (errorMsg)
            {
                error = Error(errorMsg);
                sqlite3_free(errorMsg);
            }
            callback(error);
        });
        t.detach();
    };
}

Async<Failable<vector<string>>> Database::SelectFirstCol(string query)
{
    return MakeFuncAsync(make_function([this, query]() -> Failable<vector<string>>
    {
        char* errorMsg = NULL;
        vector<string> firstColumnValues;
        auto ProcessColumns = [](void* context, int numColumns, char** columnValues, char** columnNames)
        {
            if (numColumns != 1)
            {
                return 1;
            }
            string firstColumnValue(columnValues[0]);
            auto& firstColumnValues = *static_cast<vector<string>*>(context);
            firstColumnValues.push_back(firstColumnValue);
            return 0;
        };
        sqlite3_exec(
            m_dbPtr,
            query.c_str(),
            ProcessColumns,
            &firstColumnValues,
            &errorMsg
        );
        if (errorMsg)
        {
            string errorStr(errorMsg);
            sqlite3_free(errorMsg);
            return Error(errorStr);
        }
        return firstColumnValues;
    }));
}

Async<Failable<Database::Name>> Database::GetName(string id)
{
    stringstream stream;
    stream << "CREATE TABLE IF NOT EXISTS names("
        "  id SMALLINT PRIMARY KEY,"
        "  name VARCHAR(32)"
        ");"
        "SELECT name FROM names"
        "  where id=" << id << ";";
    string query = stream.str();
    Async<Failable<vector<string>>> asyncResult = SelectFirstCol(query);
    return FunctorTransformm(asyncResult, make_function([](Failable<vector<string>> failableNames)
    {
        return FunctorBind(failableNames, make_function([](vector<string> names) -> Failable<Name>
        {
            if (names.empty())
            {
                return Error("No name found for that id.");
            }

            if (names.size() > 1)
            {
               return Error("More than one name found for that id. How is that possible!?");
            }

            return names.front();
        }));
    }));
}