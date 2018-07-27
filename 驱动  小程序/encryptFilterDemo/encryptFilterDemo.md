

data:2018.7.26

minifilter 透明加解密原理说明：

参考微软透明加解密例子：swapBuffer进行编写
	
	1、在过滤驱动安装上，先进行卷的上下文的绑定。在read和write中，处理IRP_NOCACHE（非高速缓存的I/O）标记的时候，需要用到。
	2、在读取文件Pre操作中，现将文件的buffer换成自己申请的。并将Buffer地址保存起来，在post中进行释放。(FLTFL_CALLBACK_DATA_IRP_OPERATION)进行额外的处理。
	3、在读取文件post操作中，将读取到的内容，解密，复制到用户空间中。（FLTFL_CALLBACK_DATA_IRP_OPERATION）进行特殊的处理(使用FltDoCompletionProcessingWhenSafe函数回调)
	4、在pre写的时候，将写操作的内容替换成我们自己申请的空间，进行加密。然后执行该irp
	5、在POST写的时候，将在pre写中申请的资源，进行释放。
	
	
	----------
	加解密标志：
		楚狂人编写的 sfilter 中使用的是文件件的FCB
		在miniFilter框架中，使用stream context来代替FCB（https://blog.csdn.net/wxyyxc1992/article/details/27842841）
	
	