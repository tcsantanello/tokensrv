
CREATE OR REPLACE FUNCTION user_limit( _userid integer, _vault varchar, _requests integer ) RETURNS int AS $$
DECLARE
  config  user_limits_config%ROWTYPE;
  limits  user_limits%ROWTYPE;
  allowed integer := _requests;
BEGIN
  --
  -- First get the rate limit configuration
  --
  SELECT ulc.*
    INTO config
    FROM user_limits_config ulc
   INNER JOIN user_vaults uv ON uv.id = ulc.uvault_id
   INNER JOIN vaults      v  ON v.id  = uv.id
   WHERE uv.userid = _userid
     AND _vault IN ( v.alias, v.tablename );

  IF FOUND THEN -- If there is a config, start adjusting limits
    --
    --  Get the current values
    --
    SELECT ul.*
      INTO limits
      FROM user_limits ul
     WHERE ul.config_id = config.id
       FOR UPDATE;

    IF NOT FOUND THEN
      --
      -- If not found; this is the first...
      --   adjust config values and save
      --
      limits.config_id := config.id;
      limits.expire    := now( ) + config.period;
      limits.value     := config.value - _requests;

      IF limits.value < 0 THEN
        allowed      := config.value;
        limits.value := 0;
      END IF;

      INSERT INTO user_limits ( config_id, expire, value ) VALUES ( limits.config_id, limits.expire, limits.value );
    ELSE
      --
      -- If found; update the existing value
      --
      IF limits.expire < now( ) THEN
        limits.expire := now( ) + config.period;
        limits.value  := config.value;
      END IF;

      limits.value := limits.value - _requests;

      IF limits.value < 0 THEN
        allowed      := allowed + limits.value;
        limits.value := 0;
      END IF;

      UPDATE user_limits
         SET expire = limits.expire,
             value  = limits.value
       WHERE config_id = limits.config_id;

    END IF;
  END IF;

  RETURN allowed;
END $$ LANGUAGE plpgsql;
