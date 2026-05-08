# Building UDP tileset

Currently https://github.com/Theawesomeboophis working on UndeadPeople tileset.

# His discord server: https://discord.gg/ftgMS5Rcsd

> How do I set up compose.py and properly use unpacked?

1. Fork repository https://github.com/Theawesomeboophis/UndeadPeopleUnpacked . It contains unpacked
   tileset.
2. https://www.python.org/downloads/ . Use latest version. Make sure to check "Add Python to PATH"
   in installer settings.
3. Install Libvips. Download it here: https://github.com/libvips/build-win64-mxe/releases . Grab
   vips-dev-w64-all-8.11.3.zip (or different version of it). Extract it to some directory (For
   example `C:\vips`) and add `C:\vips\bin` to Windows PATH. Instruction on how to add things to
   path look here: https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/ (I am
   using different directory for Libvips)
   ![screenshot 2021-09-13 001](https://user-images.githubusercontent.com/17512620/133093842-ef200cef-898a-4b5b-8a8e-23588e768483.png)

4. Install pyvips. To do it just write in your console: pip install pyvips
   ![screenshot 2021-09-13 003](https://user-images.githubusercontent.com/17512620/133094097-04750819-c729-473c-a1c9-f87b00e5bf9c.png)

And you are done. Now compose.py will work.

Final:

After that run `\UndeadPeopleUnpacked\!COMPOSE_MAIN.bat` to see if it is packing tileset. You will
find packed files in `UndeadPeopleUnpacked\!dda\` if everything was setup right. It will take some
time

![screenshot 2021-09-13 004](https://user-images.githubusercontent.com/17512620/133094442-30e28aad-4304-4710-8674-7314f2987473.png)

![screenshot 2021-09-13 005](https://user-images.githubusercontent.com/17512620/133094858-e123f137-a8e9-4d69-bae4-d28c065d9a81.png)

And here is packed tileset.
