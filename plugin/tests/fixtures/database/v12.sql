PRAGMA user_version = 3;
CREATE TABLE usage_snapshots (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 timestamp DATETIME DEFAULT (datetime('now')),
 provider TEXT NOT NULL,
 model TEXT DEFAULT '', input_tokens INTEGER DEFAULT 0, output_tokens INTEGER DEFAULT 0,
 request_count INTEGER DEFAULT 0, cost REAL DEFAULT 0.0, is_estimated_cost INTEGER DEFAULT 0,
 daily_cost REAL DEFAULT 0.0, monthly_cost REAL DEFAULT 0.0,
 rl_requests INTEGER DEFAULT 0, rl_requests_remaining INTEGER DEFAULT 0,
 rl_tokens INTEGER DEFAULT 0, rl_tokens_remaining INTEGER DEFAULT 0,
 cost_source TEXT NOT NULL DEFAULT 'unknown', usage_source TEXT NOT NULL DEFAULT 'unknown',
 currency TEXT DEFAULT 'USD', data_quality TEXT DEFAULT 'unknown'
);
INSERT INTO usage_snapshots(provider,model,input_tokens,output_tokens,request_count,cost,cost_source,usage_source,currency,data_quality)
VALUES('OpenRouter','fixture-model',10,5,1,2.75,'usage_api','actual_api','USD','actual');
CREATE TABLE observations (
 id INTEGER PRIMARY KEY AUTOINCREMENT, provider TEXT NOT NULL,
 observed_at_utc DATETIME NOT NULL DEFAULT (datetime('now')),
 interval_start_utc DATETIME, interval_end_utc DATETIME,
 metric_kind TEXT NOT NULL, unit TEXT NOT NULL, value REAL NOT NULL, currency TEXT,
 semantic TEXT NOT NULL CHECK(semantic IN ('gauge','cumulative_counter','interval_total','local_estimate')),
 source TEXT NOT NULL, data_quality TEXT NOT NULL DEFAULT 'unknown',
 model_scope TEXT DEFAULT '', project_scope TEXT DEFAULT '', correlation_id TEXT NOT NULL
);
INSERT INTO observations(provider,metric_kind,unit,value,currency,semantic,source,data_quality,model_scope,correlation_id)
VALUES('OpenRouter','cost','USD',2.75,'USD','gauge','actual_api','actual','fixture-model','v12-fixture');
