DROP TABLE IF EXISTS description CASCADE;
DROP TABLE IF EXISTS landmarks CASCADE;

CREATE TABLE description
(
  name character(255),
  desc_id character(7),
  address character(255),
  date_built character(10),
  architect character(255),
  landmark date
)
WITH (
  OIDS=TRUE
);

CREATE TABLE landmarks
(
  type character(1),
  latitude numeric,
  longitude numeric,
  eos character varying(10),
  sym character varying(255),
  color character(7),
  scale character(255),
  width character(255),
  opacity character(255),
  shortname character varying(255),
  description text,
  fill_color character(7),
  fill_opacity character(255),
  land_id character(5)
)
WITH (
  OIDS=TRUE
);
