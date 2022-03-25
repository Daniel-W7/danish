# danioc

danioc(Daniel's Open Console)是一个基于 gtk-3.0 的开源免费的 ssh 连接管理工具，之后计划加上sftp的功能

GITHUB地址：https://github.com/Daniel-W7/danioc

GITee地址：https://gitee.com/daniel-w7/danioc

基于jnXssh进行开发，源程序适配的系统比较老了，进行了一些修改，源程序地址：https://github.com/chrisniu1984/jnXssh

1、运行所需

	对于Debian 11 需要安装以下额外包:

	libtinyxml2.6.2v5

	安装命令：

		sudo apt-get install libtinyxml2.6.2v5

2、编译安装所需

	对于Debian 11 需要安装以下额外包:

	gcc

	make

	g++
	
	libtinyxml-dev
	
	libgtk-3-dev

	libvte-2.91-dev

	desktop-file-utils

	安装命令：

		sudo apt-get install libtinyxml-dev libgtk-3-dev libvte-2.91-dev gcc make g++ desktop-file-utils

3、编译方法
	(1)手动编译
	
	cd danioc
	
	make && make install

	(2)自动编译
	
	cd danioc
	
	./automk.sh

4、配置文件路径

	~/.danioc/site.xml

5、启动方法
	
	danioc
