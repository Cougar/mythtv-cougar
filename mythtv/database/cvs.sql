USE mythconverg;

ALTER TABLE capturecard ADD COLUMN hostname VARCHAR(255);
ALTER TABLE recorded ADD COLUMN hostname VARCHAR(255);
