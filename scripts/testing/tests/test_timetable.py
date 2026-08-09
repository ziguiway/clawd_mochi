from data.import_gdufs_timetable import DEFAULT_PERIODS, load_names, normalize_lines, parse_block


def test_normalize_lines_removes_blank_and_separator_rows():
    assert normalize_lines(" A\n\n----\n B\xa0") == ["A", "B"]


def test_parse_block_maps_course_and_supports_chinese_labels():
    courses = parse_block(
        "机器学习\n1-16周 [03-04节]\n教师：陈老师\n地点：实验楼 N301",
        3, load_names(), DEFAULT_PERIODS,
    )
    assert courses == [{
        "sourceName": "机器学习", "englishName": "Machine Learning",
        "displayName": "MACHINE LEARNING", "shortName": "ML", "day": 3,
        "weeks": "1-16", "start": "10:25", "end": "12:00",
        "room": "实验楼 N301", "teacher": "陈老师",
    }]


def test_parse_block_rejects_unknown_period():
    try:
        parse_block("课程\n1-13节", 1, {}, DEFAULT_PERIODS)
    except ValueError as error:
        assert "missing period time" in str(error)
    else:
        raise AssertionError("unknown period should fail loudly")
