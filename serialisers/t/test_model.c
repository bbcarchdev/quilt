#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "CUnit/Basic.h"
#include "../model.h"

int init_suite(void)
{
    return 0;
}

int clean_suite(void)
{
    return 0;
}

void test_sort_items_by_index(void)
{
    printf("\nTest sort_items_by_index\n");
    int size = 3;
    json_t *items[size];
    for (int i=0; i<size; i++) {
        items[i] = json_object();
        char index[80];
        sprintf(index, "%d", size-i);
        json_object_set(items[i], "index", json_string(index));
        char *value = json_string_value(json_object_get(items[i], "index"));
        printf("test data: item[%d] = %s\n", i, value);
        }

    sort_items_by_index(items, size);

    CU_ASSERT(strcmp(json_string_value(json_object_get(items[0], "index")), "1") == 0);
    CU_ASSERT(strcmp(json_string_value(json_object_get(items[1], "index")), "2") == 0);
    CU_ASSERT(strcmp(json_string_value(json_object_get(items[2], "index")), "3") == 0);
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

   if (NULL == CU_add_test(pSuite, "test sort_items_by_index", test_sort_items_by_index))
   {
      CU_cleanup_registry();
      return CU_get_error();
   }

   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   CU_cleanup_registry();
   return CU_get_error();
}
