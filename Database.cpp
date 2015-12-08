#include "Database.h"

#include <sstream>
#include <iostream>
#include <vector>

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

Error Database::ReplaceName(string id, Database::Name name)
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
		
	return Error(errorMsg);
}

Failable<Database::Name> Database::GetName(string id)
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
		return Error(errorMsg);
	}

	if (sqlNames.empty())
	{
		return Error("No name found for that id.");
	}

	if (sqlNames.size() > 1)
	{
		return Error("More than one name found for that id. How is that possible!?");
	}

	return Name(sqlNames.front());
}