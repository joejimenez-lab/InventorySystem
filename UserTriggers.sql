DROP TRIGGER IF EXISTS trigger_create_user_genre ON users;
DROP TRIGGER IF EXISTS set_update_time ON users;
DROP TRIGGER IF EXISTS set_update_time_userGenres ON userGenres;
DROP FUNCTION IF EXISTS create_user_genre();
DROP FUNCTION IF EXISTS update_time();


CREATE OR REPLACE FUNCTION update_time()
RETURNS TRIGGER AS $$
BEGIN 
    NEW.updated_time = CURRENT_TIMESTAMP;
    RETURN NEW;
END;

$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION create_user_genre()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO userGenres(user_id, genres) VALUES (NEW.id, '');
    RETURN NEW;
END;

$$ LANGUAGE plpgsql;

CREATE TRIGGER set_update_time
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_time();

CREATE TRIGGER set_update_time_userGenres
BEFORE UPDATE ON userGenres
FOR EACH ROW
EXECUTE FUNCTION update_time();

CREATE TRIGGER trigger_create_user_genre
AFTER INSERT ON users
FOR EACH ROW
EXECUTE FUNCTION create_user_genre();