# danish
danish 是一个基于 gtk-3.0 的 ssh 连接管理工具，之后计划加上sftp的功能

GITHUB地址：https://github.com/Daniel-W7/danish

GITee地址：https://gitee.com/daniel-w7/danish

基于jnXssh进行开发，源程序适配的系统比较老了，进行了一些修改，源程序地址：https://github.com/chrisniu1984/jnXssh

1、运行所需

	对于Debian 11 需要安装以下额外包:

	libtinyxml2.6.2v5

	安装命令：

		sudo apt-get install libtinyxml2.6.2v5

2、编译所需

	对于Debian 11 需要安装以下额外包:

	gcc

	make

	g++
	
	libtinyxml-dev
	
	libgtk-3-dev

	libvte-2.91-dev

	安装命令：

		sudo apt-get install libtinyxml-dev libgtk-3-dev libvte-2.91-dev gcc make g++

3、编译方法
	
	cd danish
	
	make && make install

4、配置文件路径

	~/.danish/site.xml
