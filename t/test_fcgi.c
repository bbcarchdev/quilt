#include <stdio.h>
#include <string.h>
#include "CUnit/Basic.h"

int init_suite(void)
{
    return 0;
}

int clean_suite(void)
{
    return 0;
}

void test_param_multi(void)
{
    CU_ASSERT(0 == 0);
}

int main()
{
   CU_pSuite pSuite = NULL;

   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   pSuite = CU_add_suite("Quilt_Test", init_suite, clean_suite);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   if (NULL == CU_add_test(pSuite, "test param_multi", test_param_multi))
   {
      CU_cleanup_registry();
      return CU_get_error();
   }

   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   CU_cleanup_registry();
   return CU_get_error();
}
