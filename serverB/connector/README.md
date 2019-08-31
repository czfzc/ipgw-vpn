# NEU-ipgw-flooder
A command line tool which can help you connected to NEU ipgw network control gateway through flood attack.
## Warning
The software is an open source project and licenced under MIT licence.

The software is only for communication and research purpose, do not abuse or distribute!

# **Do not abuse !!!!!**
## Build
The software no longer needs libcurl4-openssl-dev package, you don't need to install it first before trying to build the software.

There's a [easy_socket](https://github.com/KMakowsky/Socket.cpp) module in the project. Thanks to the author. I have already put the source file to my project, so you 
don't need to download.

This project needs a module [cxxopt](https://github.com/jarro2783/cxxopts) to parse args.
## Requirements
The binary seems don't rely on any extra library because the cxxopt is a head-only module. If your linux distribution can't use it, please report an issue.

## Usage
Every time using this programme include two stages. First, flood ipgw with head-only requests, Second, try one-by-one of the username list.
The help page is detailed enough to explain each specific parameter's usage.
1. First, the programme tries to send head-only requests to ipgw, use "**-m**" mode and send "**-t**" times. Use "**-f**" as index.
There are two modes to use, "single-user"(su) mode and "user range"(ur) mode. The "single-user" mode means the programme will only flood ipgw with 
specific index user several times. The "user range" mode means the programme will flood ipgw with specific index and index will auto increase until times. 
If the flood index not set, it will be a random value.

    If you don't set a "-t" option, it will be 60 by default.
2. Then, the programme will start trying to login with index specified by "**-l**", but before trying login, it must make sure the flood is enough.
So the programme will send login requests from login index and wait for a response until the response shows the previous flood is successful.

    The login index will be the same as flood index if not specific. And if you don't set a login index, the programme will continue trying to log in until 
    successful, instead of waiting until response doesn't show "must login with Union authentic".

3. If you set login index, no matter what result our programme reached, the programme must log in with your specified login index(if set), and avoid trying login with
your specified "**-g**" ignore index. If the login process with the specified login index encounters a problem, the programme will report it and stop.  
***
## The theory behind it...
There is a bug in NEU ipgw gateway. After several requests, the ipgw gateway may let you log in as a normal user without Union Authentication.
This leak can be used as a handy way to get network access through the gateway.

There are a lot of usernames stored in usernumberlist.hpp, all these usernames are possible to have the same password as the username number.
We firstly flood the ipgw gateway with plenty of requests. And when the response page no more contains "you must log in via union authentic", 
then we can try a username with the same password and log in. Fortunately, this will make you login successfully.

If there are other problems such as insufficient funds, you can quickly pick another account and try again. After a few attempts, it will succeed.

This operation is very useful when you want to log in anonymously. And if you want to use this programme to save some expenses on network fee,
this programme may not suitable for you due to it takes a long time to flood firstly. And there is no good for you to flood the NEU web service as well.

By the way, the usernumberlist will shrink due to password error. This is a deadly damage. Please contribute more student's user number which have the same
password. Arigato!

So, do not abuse the programme. And, don't share the usernumberlist. Although there is supposed to be no way to change their original password,
 the operation is still possible. If possible, please report the bug to NEU web center, thank you very much.
## Finally
I want to compile the project under windows environment. So the windows release will be published after several commits.
Feel free to compile the source code in any other platform, let's make things easier and easier.

There is still a problem whether making uncomplete requests will be faster than making a full request. The program is not write 
as a multi-thread program because it is dangerous and complex, and is quite harmful to the gateway server as well.
For you user, feel free to use the program and report bugs positively, for our developer, there is nothing can block me to **love Misaka Mikoto**!

#### 君の指先で舞っている電光は、僕の今生に変わらない信仰！
I love Toaru kagaku no railgun, and I think the program just like a **RAILGUN** breaking the jail 
set up by NEU and let you perform any hacker operation as you wish. So cool!!!!

The electric sparkle glittering on your fingertips is my rock solid faith for life!

**_Only my railgun!!!!_**

![](http://getwallpapers.com/wallpaper/full/0/e/3/923293-new-emily-the-strange-wallpaper-2676x1440.jpg)
