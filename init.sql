CREATE TABLE hosts(
	hosts_id SERIAL PRIMARY KEY,
	info TEXT NOT NULL,
	ip INET NOT NULL,
	port INT NOT NULL CHECK (port BETWEEN 1 AND 65535),
	UNIQUE (ip, port)
);

CREATE TABLE host_status(
	hosts_id INT PRIMARY KEY REFERENCES hosts(hosts_id) ON DELETE CASCADE,	
	down_since TIMESTAMPTZ DEFAULT NULL,
	avg_latency REAL,
	max_latency INT,
	min_latency INT
);

CREATE TABLE metrics(
	metrics_id SERIAL PRIMARY KEY,
	hosts_id INT NOT NULL REFERENCES hosts(hosts_id) ON DELETE CASCADE,	
	status BOOL NOT NULL,
	latency INT,
	ts TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE clear_rules(
	hosts_id INT PRIMARY KEY REFERENCES hosts(hosts_id) ON DELETE CASCADE,
	retain_for INTERVAL NOT NULL DEFAULT '30 seconds'
);

CREATE TABLE archive(
	archive_id SERIAL PRIMARY KEY,
	info TEXT NOT NULL,
	ip INET NOT NULL,
	port INT NOT NULL CHECK (port BETWEEN 1 AND 65535),	
	status BOOL NOT NULL,
	latency INT,
	ts TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE user_logs(
	user_logs_id SERIAL PRIMARY KEY,
	action_type TEXT,
	ts TIMESTAMPTZ NOT NULL DEFAULT now(),
	details TEXT
);