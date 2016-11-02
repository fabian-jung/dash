#include <libdash.h>
#include <gtest/gtest.h>

#include <string>

#include "TestBase.h"
#include "DARTLocalityTest.h"

bool domains_are_equal(
  const dart_domain_locality_t * loc_a,
  const dart_domain_locality_t * loc_b)
{
  if (loc_a->num_domains    != loc_b->num_domains ||
      loc_a->num_units      != loc_b->num_units ||
      loc_a->num_cores      != loc_b->num_cores ||
      loc_a->num_nodes      != loc_b->num_nodes ||
      loc_a->scope          != loc_b->scope ||
      loc_a->level          != loc_b->level ||
      loc_a->global_index   != loc_b->global_index ||
      loc_a->relative_index != loc_b->relative_index ||
      loc_a->team           != loc_b->team) {
    return false;
  }
  if (std::string(loc_a->domain_tag) != std::string(loc_b->domain_tag)) {
    return false;
  }
  if (std::string(loc_a->host) != std::string(loc_b->host)) {
    return false;
  }
  for (int u = 0; u < loc_a->num_units; u++) {
    if (loc_a->unit_ids[u] != loc_b->unit_ids[u]) { return false; }
  }
  for (int d = 0; d < loc_a->num_domains; d++) {
    EXPECT_EQ_U(loc_a, loc_a->domains[d].parent);
    EXPECT_EQ_U(loc_b, loc_b->domains[d].parent);
    EXPECT_EQ_U(d, loc_a->domains[d].relative_index);
    EXPECT_EQ_U(d, loc_b->domains[d].relative_index);
    if (!domains_are_equal(&loc_a->domains[d], &loc_b->domains[d])) {
      return false;
    }
  }
  return true;
}

TEST_F(DARTLocalityTest, CloneLocalityDomain)
{
  dart_domain_locality_t * loc_team_all_orig;
  EXPECT_EQ_U(
    dart_domain_team_locality(DART_TEAM_ALL, ".", &loc_team_all_orig),
    DART_OK);

  // Create copy of global locality domain:
  dart_domain_locality_t * loc_team_all_copy;
  EXPECT_EQ_U(
    dart_domain_clone(loc_team_all_orig, &loc_team_all_copy),
    DART_OK);

  // Compare attributes of original and copied locality domains:
  EXPECT_EQ_U(true, domains_are_equal(loc_team_all_orig, loc_team_all_copy));

  dart_domain_destruct(loc_team_all_copy);
}

TEST_F(DARTLocalityTest, FindLocalityDomain)
{
  dart_domain_locality_t * loc_team_all_orig;
  EXPECT_EQ_U(
    dart_domain_team_locality(DART_TEAM_ALL, ".", &loc_team_all_orig),
    DART_OK);
}


TEST_F(DARTLocalityTest, UnitLocality)
{
  DASH_LOG_TRACE("DARTLocalityTest.Domains",
                 "get local unit locality descriptor");
  dart_unit_locality_t * ul;
  ASSERT_EQ_U(DART_OK, dart_unit_locality(DART_TEAM_ALL, _dash_id, &ul));
  DASH_LOG_TRACE("DARTLocalityTest.Domains",
                 "pointer to local unit locality descriptor:", ul);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", *ul);

  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->unit);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->domain_tag);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.host);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.numa_id);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.cpu_id);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.num_cores);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.min_cpu_mhz);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.max_cpu_mhz);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.min_threads);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", ul->hwinfo.max_threads);

  EXPECT_EQ_U(_dash_id, ul->unit);

  // Units may group multiple cores:
  EXPECT_GE_U(ul->hwinfo.cpu_id,      0);
  EXPECT_GT_U(ul->hwinfo.num_cores,   0);
  EXPECT_GT_U(ul->hwinfo.min_threads, 0);
  EXPECT_GT_U(ul->hwinfo.max_threads, 0);

  // Get domain locality from unit locality descriptor:
  DASH_LOG_TRACE("DARTLocalityTest.UnitLocality",
                 "get local unit's domain descriptor");
  dart_domain_locality_t * dl;
  ASSERT_EQ_U(
    DART_OK,
    dart_domain_team_locality(DART_TEAM_ALL, ul->domain_tag, &dl));
  DASH_LOG_TRACE("DARTLocalityTest.UnitLocality",
                 "pointer to local unit's domain descriptor:", dl);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.UnitLocality", *dl);

  EXPECT_GT_U(dl->level, 2);
  EXPECT_EQ_U(dl->scope, DART_LOCALITY_SCOPE_CORE);
}

TEST_F(DARTLocalityTest, Domains)
{
  DASH_LOG_TRACE("DARTLocalityTest.Domains",
                 "get global domain descriptor");
  dart_domain_locality_t * dl;
  ASSERT_EQ_U(DART_OK, dart_domain_team_locality(DART_TEAM_ALL, ".", &dl));
  DASH_LOG_TRACE("DARTLocalityTest.Domains",
                 "pointer to global domain descriptor: ", dl);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.Domains", *dl);

  DASH_LOG_TRACE_VAR("DARTLocalityTest.Domains", dl->domain_tag);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.Domains", dl->level);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.Domains", dl->num_domains);
  DASH_LOG_TRACE_VAR("DARTLocalityTest.Domains", dl->num_nodes);

  // Global domain has locality level 0 (DART_LOCALITY_SCOPE_GLOBAL):
  EXPECT_EQ_U(dl->level, 0);
  EXPECT_EQ_U(dl->level, DART_LOCALITY_SCOPE_GLOBAL);
}
