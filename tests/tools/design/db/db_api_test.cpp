/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/db/DbException.hpp"
#include "tools/design/db/IDatabase.hpp"
#include "tools/design/db/IMigrator.hpp"
#include "tools/design/db/IResultSet.hpp"
#include "tools/design/db/ISecretProvider.hpp"
#include "tools/design/db/IStatement.hpp"
#include "tools/design/db/ITransaction.hpp"
#include "tools/design/db/ScopedTransaction.hpp"

#include <boost/test/unit_test.hpp>

#include <type_traits>

using namespace tools::design::db;

BOOST_AUTO_TEST_CASE(db_headers_are_abstract_interfaces)
{
    BOOST_CHECK(std::is_abstract_v<IDatabase>);
    BOOST_CHECK(std::is_abstract_v<IStatement>);
    BOOST_CHECK(std::is_abstract_v<IResultSet>);
    BOOST_CHECK(std::is_abstract_v<IRow>);
    BOOST_CHECK(std::is_abstract_v<ITransaction>);
    BOOST_CHECK(std::is_abstract_v<IMigrator>);
    BOOST_CHECK(std::is_abstract_v<ISecretProvider>);
    BOOST_CHECK((std::is_base_of_v<std::runtime_error, DbException>));
    BOOST_CHECK((!std::is_abstract_v<ScopedTransaction>));
}
