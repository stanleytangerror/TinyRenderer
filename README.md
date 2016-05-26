# TinyRenderer

学习图形学而写的3D软件渲染器（正在构建中）

特性
===
* programmable pipeline，通过C++模板实现vertex shader和fragment shader
* VS2015 x86或x64下build
* 依赖Eigen 3.2.6
* 渲染结果输出部分使用[skywind3000/mini3d](https://github.com/skywind3000/mini3d)的device_t和screen部分

截图
===

透视正确的mipmap纹理

![](.//images//mipmap.png)

顶点平均化法线，Phong光照

![](.//images//phong.png)


致谢
===
受[skywind3000](https://github.com/skywind3000)的许多启发

