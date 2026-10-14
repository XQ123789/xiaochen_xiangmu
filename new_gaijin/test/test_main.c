/**
 * @file    test_main.c
 * @brief   测试运行器 - 调用所有测试套件，输出通过/失败统计
 * @note    纯算法模块单元测试入口
 *
 * 编译命令（GCC）：
 *   gcc -I../include -o test_all test_main.c test_coord.c test_gps_parser.c \
 *       test_json_builder.c ../src/ng_coord.c ../src/ng_gps_parser.c \
 *       ../src/ng_json_builder.c -lm
 */

#include <stdio.h>

/* 外部测试套件入口函数 */
extern int test_coord_run(void);
extern int test_gps_parser_run(void);
extern int test_json_builder_run(void);

int main(void)
{
    int total_fail = 0;

    printf("========================================\n");
    printf("  GPS定位项目 - 纯算法模块单元测试\n");
    printf("  版本: v%d.%d.%d\n", 1, 0, 0);
    printf("========================================\n");

    /* 运行各测试套件 */
    total_fail += test_coord_run();
    total_fail += test_gps_parser_run();
    total_fail += test_json_builder_run();

    /* 输出总结 */
    printf("\n========================================\n");
    if (total_fail == 0) {
        printf("  所有测试通过！\n");
    } else {
        printf("  共 %d 个测试失败\n", total_fail);
    }
    printf("========================================\n");

    return (total_fail == 0) ? 0 : 1;
}
