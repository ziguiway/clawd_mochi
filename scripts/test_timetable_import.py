#!/usr/bin/env python3
"""课表导入器的离线正向测试，不访问教务系统或设备。"""

from import_gdufs_timetable import DEFAULT_PERIODS, load_names, parse_block


def main() -> None:
    names = load_names()
    sample = """机器学习
1-16周 [03-04节]
教师：陈老师
地点：实验楼 N301
---------------------
高级算法设计与分析
2-14周 [07-08节]
教师：李老师
教室：B205"""
    courses = parse_block(sample, 3, names, DEFAULT_PERIODS)
    assert len(courses) == 2
    assert courses[0]["displayName"] == "MACHINE LEARNING"
    assert courses[0]["shortName"] == "ML"
    assert courses[0]["day"] == 3
    assert courses[0]["weeks"] == "1-16"
    assert courses[0]["start"] == DEFAULT_PERIODS["3"][0]
    assert courses[0]["end"] == DEFAULT_PERIODS["4"][1]
    assert courses[0]["room"] == "实验楼 N301"
    assert courses[1]["displayName"] == "ADV. ALGORITHMS"
    assert courses[1]["teacher"] == "李老师"
    print("PASS  GDUFS timetable parser and graduate CS name mapping")


if __name__ == "__main__":
    main()
