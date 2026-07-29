PRAGMA user_version = 4;
CREATE TABLE usage_snapshots (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 timestamp DATETIME DEFAULT (datetime('now')),
 provider TEXT NOT NULL,
 model TEXT DEFAULT '',
 input_tokens INTEGER DEFAULT 0,
 output_tokens INTEGER DEFAULT 0,
 request_count INTEGER DEFAULT 0,
 cost REAL DEFAULT 0.0,
 is_estimated_cost INTEGER DEFAULT 0,
 daily_cost REAL DEFAULT 0.0,
 monthly_cost REAL DEFAULT 0.0,
 rl_requests INTEGER DEFAULT 0,
 rl_requests_remaining INTEGER DEFAULT 0,
 rl_tokens INTEGER DEFAULT 0,
 rl_tokens_remaining INTEGER DEFAULT 0,
 cost_source TEXT NOT NULL DEFAULT 'unknown',
 usage_source TEXT NOT NULL DEFAULT 'unknown',
 currency TEXT DEFAULT 'USD',
 data_quality TEXT DEFAULT 'unknown'
);
INSERT INTO usage_snapshots(
 provider,model,input_tokens,output_tokens,request_count,cost,
 is_estimated_cost,daily_cost,monthly_cost,cost_source,usage_source,currency,
 data_quality
) VALUES(
 'Anthropic','claude-fixture',250,50,4,3.25,0,3.25,17.50,
 'billing_api','usage_api','USD','actual'
);
CREATE TABLE observations (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 provider TEXT NOT NULL,
 observed_at_utc DATETIME NOT NULL DEFAULT (datetime('now')),
 interval_start_utc DATETIME,
 interval_end_utc DATETIME,
 metric_kind TEXT NOT NULL,
 unit TEXT NOT NULL,
 value REAL NULL,
 currency TEXT,
 semantic TEXT NOT NULL CHECK(semantic IN (
  'gauge','cumulative_counter','interval_total','local_estimate'
 )),
 source TEXT NOT NULL,
 data_quality TEXT NOT NULL DEFAULT 'unknown',
 scope TEXT NOT NULL DEFAULT 'api_key',
 window TEXT NOT NULL DEFAULT 'current',
 model_scope TEXT DEFAULT '',
 project_scope TEXT DEFAULT '',
 reset_at_utc DATETIME,
 correlation_id TEXT NOT NULL
);
INSERT INTO observations(
 provider,observed_at_utc,metric_kind,unit,value,currency,semantic,source,
 data_quality,scope,window,model_scope,project_scope,correlation_id
) VALUES(
 'Anthropic','2026-07-28 12:00:00','cost','USD',3.25,'USD','gauge',
 'billing_api','actual','account','calendar_month','claude-fixture',
 'workspace-fixture','v16-fixture'
);
CREATE INDEX idx_observations_provider_time_source_currency
 ON observations(provider, observed_at_utc, source, currency);
