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

## SQL优化
```
1.sql分页
解决大量结果返回问题,用limit对于前端返回的页码(page)和页大小(size)对sql进行限制,即limit (page-1)*size,size
但这样对于靠后的页码查询会越来越慢,接近count(1)效率,原因是定位起始字段(limit (page-1)*size))造成了全表扫描。
若查询字段（如id)走索引,则可利用到B+树默认有序的特性,让前端返回上一页码的id的最大条目max_id,对当前页码查询进行限制
即select xxx from xxx where id > max_id limit (page-1)*size,size

2.sql连接池
sql查询过程:
tcp三次握手，身份安全检验，查询结果，返回结果，四次挥手
如果提前创建连接池，可以提前创建连接,查询时直接查询结果和返回结果，提高查询速度。
pymysql连接池使用技巧：
https://blog.csdn.net/wtt234/article/details/127939825
https://blog.csdn.net/diuleilaomu/article/details/103278147

3.模糊搜索优化
like如果不是左前缀，不会走索引，造成慢查询。
优化方法：instr进行过滤并配合like
https://m.jb51.net/article/231867.htm

4.group by实现原理和优化
group by默认会进行排序 + 分组
或者 分组 + 排序
如果group by的字段为索引,大部分情况会走索引,从而不需要排序,提高查询效率
可以显式指定不排序,例group by id order by null,参考
https://blog.csdn.net/Mango_Bin/article/details/122621985

5.避免使用select * from xxx,这样不会走索引

6.通常主键为聚簇索引，而辅键位非聚簇索引,索引查询会通过辅键查询到主键id,再通过主键索引查询id取出行数据,该过程为回表。
合理利用索引,不合理的索引会造成大量回表操作，甚至降低查询效率

7.多使用explain查看执行计划,排查慢查询原因
```
