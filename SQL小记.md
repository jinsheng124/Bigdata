# SQL小记：

## 关联更新SQL:

以student_record为例
```
//先增加新列class
alter table student_record add class tinyint default null comment "班级"; 

//将表student_record的class字段更新为表map_id_class的class字段，关联字段id

update student_record a,map_id_class b set a.class=b.class where a.id = b.id; 
```

## 删除重复数据,只保留一行：

以删除ttest为例，表结构如下：

|name[varchar(20)]|age[int(4)]|
|---|---|
|tom|18|
|tom|18|
|tom|18|
|jack|20|
|jack|20|
|Lily|30|
|mary|75|
|mary|75|
|mary|75|
|mary|75|
|mary|75|
```
1.先创建一个带有行序号的副本ttest2

create table ttest2 select *,row_number() over() as ranks from ttest;

2.取消安全模式

SET SQL_SAFE_UPDATES = 0;

3.删除多余的重复数据，只保留ranks值最小的结果

delete from ttest2 where 
ranks not in (select * from (select min(ranks) from ttest2 group by name having count(*)>1) m)
and name in (select * from (select name from ttest2 group by name having count(*)>1) n);

4.删除原表ttest，更新表名ttest2为ttest,并去掉行序号

drop table ttest;
alter table ttest2 rename to ttest;
alter table ttest drop column ranks;
```

## 外键约束规则：
```

1.创建表的时候，应该先创建被关联表(没有外键字段的表)。

2.添加数据的时候，应该先添加被关联表(没有外键字段的表)的数据。

3.外键字段添加的值只能是被关联表中已经存在的值。

3.修改、删除被关联表的数据都会出现障碍，需要优先修改、删除外键字段的值。
```

## 添加序号方式：
```
select (@a:=@a+1) as id from ttest,(select @a :=0) as init;
```
