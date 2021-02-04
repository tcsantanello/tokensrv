
CREATE OR REPLACE FUNCTION user_limit( _username varchar, vault varchar, requests integer ) RETURNS int AS $$
DECLARE
  userid integer;
BEGIN
  SELECT id INTO userid FROM user_vaults WHERE username = _username;
  RETURN user_limit( userid, vault, requests );
END $$ LANGUAGE plpgsql;
