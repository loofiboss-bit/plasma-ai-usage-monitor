PRAGMA user_version = 2;
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
INSERT INTO usage_snapshots(provider,model,input_tokens,output_tokens,request_count,cost,is_estimated_cost,daily_cost,monthly_cost,cost_source,usage_source,currency,data_quality)
VALUES('OpenAI','gpt-fixture',100,25,3,1.5,0,1.5,1.5,'billing_api','actual_api','USD','complete');
