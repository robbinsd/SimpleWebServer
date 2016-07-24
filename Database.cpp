#include "Database.h"

#include <sstream>
#include <iostream>
#include <vector>
#include <thread>

#include "include/sqlite/sqlite3.h"

#include "Choice.h"

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
        // TODO: isn't detach unsafe? should use join here instead?
        t.detach();
    };
}

Async<Failable<Database::Name>> Database::GetName(string id)
{
    // TODO: make Async2 class, where lambda returns whatever value we want to callback with.
    // This would make it super easy to turn a synchronous function into an asynchronous one.
    return [=](Continuation<Failable<Database::Name>> callback)
    {
        std::thread t([=]()
        {
            stringstream stream;
            stream << "CREATE TABLE IF NOT EXISTS names("
                "  id SMALLINT PRIMARY KEY,"
                "  name VARCHAR(32)"
                ");"
                "SELECT name FROM names"
                "  where id=" << id << ";";
            string query = stream.str();
            char* errorMsg = NULL;
            vector<Name> sqlNames;
            sqlite3_exec(
                m_dbPtr,
                query.c_str(),
                [](void* context, int numColumns, char** columnValues, char** columnNames)
                {
                    if (numColumns != 1)
                    {
                        return 1;
                    }
                    string firstColumnValue(columnValues[0]);
                    auto& responseRows = *static_cast<vector<string>*>(context);
                    responseRows.push_back(firstColumnValue);
                    return 0;
                },
                &sqlNames,
                &errorMsg);

            if (errorMsg)
            {
                callback(Error(errorMsg));
                return;
            }

            if (sqlNames.empty())
            {
                callback(Error("No name found for that id."));
                return;
            }

            if (sqlNames.size() > 1)
            {
                callback(Error("More than one name found for that id. How is that possible!?"));
                return;
            }

            callback(Name(sqlNames.front()));
        });
        // TODO: isn't detach unsafe? should use join here instead?
        t.detach();
    };
}