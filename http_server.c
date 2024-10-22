#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> 
#include <dirent.h> //新：目录操作函数
#include <sys/stat.h>//新：让程序能查询文件状态（即文件大小、权限、最后修改时间等）

#define PORT 8080
#define BUFFER_SIZE 1024

//http部分：

//处理客户端请求：
void handle_request(int client_socket) {


    char buffer[BUFFER_SIZE];            //缓冲区 ：用以存放客户端读取的数据
    int read_size = read(client_socket,buffer,BUFFER_SIZE - 1); //1、从client_socket(客户端套接字)读取数据，2、buffer提供内存区域，临时存储client数据
    buffer[read_size]='\0';                                     //3、直至读取了BUFFER_SIZE-1个字符，添加一个空字符\0，确保buffer为有效的C字符串。
    
    if (strcmp(buffer, "") == 0)
        return;
    
    printf("%s\n", buffer);//输出请求数据内容
        
    //获取当前目录
    char cwd[BUFFER_SIZE] = "";   
    getcwd(cwd, sizeof(cwd));
    
    //从http请求中提取请求路径
    char *path = strstr(buffer, "GET") + 4;     //strstr为字符串处理函数，在buffer字符串中查找第一次出现的子串"GET"，为http请求的开始；+4是将指针（strstr定位到的）向前移动4个字符，跳过GET（后面还有个空格）获取路径的开始位置。
    path[strcspn(path, " ")] = '\0';         //到达开始位置后strcspn函数发挥作用，(path,"")找到路径和协议版本之间的空格位置，在此设置为字符串结束符"\0"，将路径和协议版本分开，此路径用于确定如何处理相应客户端请求。
    
    //将当前目录和url组成默认主页路径
    char abs_path[BUFFER_SIZE] = "";
    if (strcmp(path, "/") == 0) //strcmp也为字符串处理函数，比较两个字符串是否相等，此处检查请求路径（上面定位了）是否为根路径/，若是，返回返回0（==0）。
    {   
        snprintf(abs_path, sizeof(abs_path), "%s%s", cwd, "/index.html");  //若为0，调用srtcpy函数（复制作用），服务器返回index.html文件作为默认界面。              
    }
    else {
    	snprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
    }
    //printf("Requested path: %s\n", abs_path);


    struct stat st;
    if (stat(abs_path, &st) == -1) {
        char header[256];
        snprintf(header, sizeof(header), "HTTP/1.1 404 Not Found\r\n\r\n");
        write(client_socket, header, strlen(header));
        write(client_socket, "404 Not Founding", 13);
    } 
    else {

    //HTTP响应头的构建
    FILE *file = fopen(abs_path, "r");          //定义一个文件流，打开指定路径的文件，"r"表示只读模式
    char header[256];                      //声明一个数组header，指定大小256字节，存储HTTP响应头

    //文件响应头
    if (file) {
        //snprintf是格式化数据写入字符串的函数 内部:1、响应行：版本号+状态码（200表示成功）OK+\r\n;2、末尾的\r\n\r\n分隔头部和消息体
        snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\n\r\n");
        write(client_socket, header, strlen(header)); //调用write函数，将数据写入客户端套接字即把响应头发送至客户端。
	//文件响应体
        while (fgets(buffer, BUFFER_SIZE, file)) {    //调用fget函数，从文件流中读取数据最多size-1字符 流出\0的空间，存储在buffer之中（若有错误则返回NULL）
            write(client_socket, buffer, strlen(buffer)); //把buffer中的数据写入客户端套接字即发送到客户端中。
        }
        fclose(file);  //调用fclose函数，关闭文件流
    }
    else{
     char header[256];
     snprintf(header,sizeof(header),"HTTP/1.1 500 Internal Server Error\r\n\r\n");
     write(client_socket ,header ,strlen(header));          //第一次write发送http响应头，包括状态行，及空行标志结束
     write(client_socket, "500 Internal Server Error", 23); //第二次write发送http响应体，为简单的文本信息表明未找到。
    }

    //目录响应头
    DIR *dir = opendir(path[0] == '/' ? path + 1 : "."); //打开目录，若path第一个字符是/(绝对路径)，path+1跳过当前目录；若不是,打开（.)当前目录
    if (dir) {
        char header[256];
            snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            write(client_socket, header, strlen(header));

	    //目录响应体（列出目录内容）
            const char *html_header = "<html><body><ul>";
            write(client_socket, html_header, strlen(html_header));  
            struct dirent *entry;                      //该结构体里包含目录条目的信息，通过entry指针访问结构体中的信息，同时结构体中包含下一个指向下一个结构体实例的指针，可允许遍历所有条目
            while ((entry = readdir(dir))) {           //调用readdir函数，读取目录的下一条条目
                if (entry->d_type == DT_REG) {         //调用entry->d_name 来获取条目的名称，检查其是否为普通文件
                    char line[1024];
                    snprintf(line, sizeof(line), "<li><a href=\"%s\">%s</a></li>", entry->d_name, entry->d_name);
                    write(client_socket, line, strlen(line));
                }                                      //调用fprintf，把格式化的数据写入文件流中，给客户端发送格式化的HTML内容，即双引号内的：<li>列表项；<a>链接；之后获取当前条目录的名称
                                                       //<li>:开始标;；a（锚点开始标签） herf：指定超链接目标URL %s 被替换为目标条目名称;%s同样被替换，作为超链接显示文本，即用户可点击文本;</a>与</li>为锚点和列表项结束的标签
                                                       //enter->d_name:用于获取当前目标条目名称，作为HTML连接的显示文本（用户可点击）和URL（定位）;上面代码有来年各个被替换为目标条目名称的地方，此时entry调用两次，分别将其替换
                                                       //最终生成带有超链接的列表项，用户可点击访问文件或页面
            }                                  
            write(client_socket, "</ul></body></html>", 20);//fputs向客户端发送ul结束；body结束；html结束的定义;最终构成完整的HTML页面结构，其中有一个无序列表
            closedir(dir);
        }
   else {
            // Directory access failed
            char header[256];
            snprintf(header, sizeof(header), "HTTP/1.1 403 Forbidden\r\n\r\n");
            write(client_socket, header, strlen(header));
            write(client_socket, "403 Forbidden", 13);
            printf("failed");
        }
    }
}            


 


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    printf("Hello\n");

if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
}

if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0){
  perror("socketopt falied");
  exit(EXIT_FAILURE);
}

address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(PORT);

if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
}
if (listen(server_fd, 3) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
}

printf("Server listening on port %d...\n", PORT);


while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) > 0) {
    if (new_socket < 0){
        perror("accpetion failed");
        continue;
    }
        printf("Connection established\n");
        handle_request(new_socket);
        close(new_socket);
    }

    if (new_socket < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    return 0;
}

    

