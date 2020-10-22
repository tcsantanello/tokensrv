CREATE OR REPLACE FUNCTION random_string(int)
RETURNS text
AS $$
  SELECT array_to_string(
    ARRAY (
      SELECT substring( '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ' FROM (random() *36)::int FOR 1)
        FROM generate_series(1, $1) ), '' )
$$ LANGUAGE sql;

CREATE TABLE vaults (
  format    integer,
  alias     VARCHAR(255),
  tablename VARCHAR(255),
  enckey    VARCHAR(255),
  mackey    VARCHAR(255),
  durable   boolean,

  CONSTRAINT vaults_alias_key UNIQUE ( alias ),
  CONSTRAINT vaults_name_key UNIQUE ( tablename )
);

CREATE TABLE f6l4_dur (
  token VARCHAR(20) NOT NULL,
  hmac bytea,
  crypt bytea,
  mask VARCHAR(20),
  expiration date,
  properties bytea,
  enckey VARCHAR(255),

  CONSTRAINT f6l4_dur_pkey     PRIMARY KEY ( token ),
  CONSTRAINT f6l4_dur_hmac_key UNIQUE ( hmac )
);

CREATE TABLE f6l4_tran (
  token VARCHAR(20) NOT NULL,
  hmac bytea,
  crypt bytea,
  mask VARCHAR(20),
  expiration date,
  properties bytea,
  enckey VARCHAR(255),

  CONSTRAINT f6l4_tran_tok_key UNIQUE ( token )
);

INSERT INTO vaults ( format, alias, tablename, enckey, mackey, durable )
            VALUES ( 7, 'durable', 'f6l4_dur', random_string(10), random_string(10), true );

INSERT INTO vaults ( format, alias, tablename, enckey, mackey, durable )
            VALUES ( 7, 'transactional', 'f6l4_tran', random_string(10), random_string(10), false );

CREATE TABLE users (
  id       SERIAL PRIMARY KEY,
  username VARCHAR(50) NOT NULL,
  password CHAR(64),
  token    VARCHAR(25),

  CONSTRAINT users_username_key UNIQUE( username ),
  CONSTRAINT users_token_key UNIQUE( token ),
  CONSTRAINT users_auth_key CHECK ( password is not null or token is not null )
);

INSERT INTO users ( username, password, token )
       VALUES ( 'user1', encode( digest( 'password', 'sha256' ), 'hex' ), 'user1token' );

CREATE TABLE user_vaults (
  id       SERIAL PRIMARY KEY,
  userid   INTEGER NOT NULL,
  vault    VARCHAR(255) NOT NULL,

  CONSTRAINT user_vaults_uid_vault UNIQUE ( userid, vault )
);

INSERT INTO user_vaults ( userid, vault )
       SELECT id, 'durable' FROM users WHERE username = 'user1';
INSERT INTO user_vaults ( userid, vault )
       SELECT id, 'transactional' FROM users WHERE username = 'user1';

CREATE TABLE user_limits_config (
  id         SERIAL PRIMARY KEY,
  uvault_id  INTEGER NOT NULL,
  period     INTERVAL NOT NULL,
  value      INTEGER NOT NULL,

  CONSTRAINT user_limit_vaults_fk FOREIGN KEY ( uvault_id ) REFERENCES user_vaults( id )
);

CREATE TABLE user_limits (
  id         SERIAL,
  config_id  INTEGER NOT NULL,
  expire     TIMESTAMP NOT NULL,
  value      INTEGER NOT NULL,

  CONSTRAINT user_limits_config_fk FOREIGN KEY ( config_id ) REFERENCES user_limits_config( id )
);
