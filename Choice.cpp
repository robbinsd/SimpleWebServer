#include "Choice.h"

std::ostream& operator<< (std::ostream& out, const Error& err)
{
    return out << "{" << err.m_message << ", " << err.m_code << "}";
}