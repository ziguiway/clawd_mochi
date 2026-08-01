"""Stable data used by the Web UI regression fixtures."""

COINLORE_ASSETS = [
    {"id": "90", "symbol": "BTC", "name": "Bitcoin", "rank": 1},
    {"id": "80", "symbol": "ETH", "name": "Ethereum", "rank": 2},
    {"id": "48543", "symbol": "SOL", "name": "Solana", "rank": 5},
    {"id": "58", "symbol": "XRP", "name": "XRP", "rank": 6},
    {"id": "2", "symbol": "DOGE", "name": "Dogecoin", "rank": 8},
    {"id": "33536", "symbol": "OKB", "name": "OKB", "rank": 31},
    {"id": "42855", "symbol": "XAUT", "name": "Tether Gold", "rank": 37},
    {"id": "70001", "symbol": "SPK", "name": "Spark", "rank": 145},
]

INITIAL_ASSETS = [
    {
        "id": "90",
        "symbol": "BTC",
        "name": "Bitcoin",
        "gold": False,
        "price": 118400.0,
        "change": 2.6,
    },
    {
        "id": "80",
        "symbol": "ETH",
        "name": "Ethereum",
        "gold": False,
        "price": 3860.0,
        "change": -0.8,
    },
    {
        "id": "42855",
        "symbol": "XAUT",
        "name": "Tether Gold",
        "gold": False,
        "price": 4073.5,
        "change": 0.8,
    },
]

INITIAL_MARKET_ASSETS = [
    {
        "secid": "1.000001",
        "code": "000001",
        "label": "SSE",
        "name": "上证指数",
        "price": 3858.25,
        "change": 1.15,
    },
    {
        "secid": "0.399001",
        "code": "399001",
        "label": "SZSE",
        "name": "深证成指",
        "price": 14148.73,
        "change": 2.72,
    },
    {
        "secid": "0.399006",
        "code": "399006",
        "label": "CYB",
        "name": "创业板指",
        "price": 3590.79,
        "change": 3.16,
    },
]

MARKET_SEARCH_RESULTS = [
    {
        "secid": "1.600519",
        "code": "600519",
        "label": "600519",
        "name": "贵州茅台",
    },
    {
        "secid": "0.000858",
        "code": "000858",
        "label": "000858",
        "name": "五粮液",
    },
]

