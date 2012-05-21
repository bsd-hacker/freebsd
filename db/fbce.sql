-- drop table if exists config cascade;
-- create table config (
--         key varchar not null primary key,
--         value varchar not null,
--         unique(key)
-- );
-- insert into config values('reg_open', '2010-06-09T00:00:00');
-- insert into config values('reg_close', '2010-06-16T00:00:00');
-- insert into config values('vote_open', '2010-06-17T00:00:00');
-- insert into config values('vote_close', '2010-07-14T00:00:00');

drop table if exists persons cascade;
create table persons (
        id serial primary key,
        login varchar not null,
        realname varchar null,
        password varchar not null,
        admin boolean not null default false,
	active boolean not null default false,
        incumbent boolean not null default false,
	voted boolean not null default false,
        votes integer not null default 0,
        unique(login)
);
insert into persons(login, realname, password, admin)
    values('des', 'Dag-Erling Smørgrav', '*', true);

drop table if exists statements cascade;
create table statements (
        id serial primary key,
        person integer not null,
        short varchar(64) not null,
        long text not null,
        unique(person),
        foreign key(person) references persons(id) on delete cascade on update cascade
);

drop table if exists votes cascade;
create table votes (
        id serial primary key,
        voter integer not null,
        candidate integer not null,
        unique(voter, candidate),
        foreign key(voter) references persons(id) on delete cascade on update cascade,
        foreign key(candidate) references persons(id) on delete cascade on update cascade
);

drop view if exists results;
create view results as
    select persons.id, persons.login as login, persons.realname as realname, persons.incumbent, count(votes.*) as votes
    from persons join votes on persons.id = votes.candidate
    group by persons.id, persons.login, persons.realname, persons.incumbent;

-- select * from results order by votes limit 9;
