---
layout: default
comments: true
categories: jekyll disqus
title: Scaling filters
# other options
---

# [](#header-1)Scaling filters

Changing the options in the QuickViewer's Rendering menu will change the look of your image.
This is because the interpolation method to reduce the image is changed.

On this page, we will explain the difference in each option.

## Summary

- Normally, in many cases **Bilinear interpolation** will be OK.
- If you are hard to read a fine letter or you are reading Japanese comics please try **Bicubic interpolation by CPU**.
- If you are dissatisfied with the above options, please try **Spline36 or Lanczos interpolation by CPU**.

## About the sample image

In the following explanation, as a sample image, a reduced image of Japanese free comic "赤き血潮に(for Red Blood)" is used.
About the use on this page, I have obtained permission from 相生青唯(AIOI Aoi) of the author.

To see the original about [赤き血潮に(www.pixiv.net)](https://www.pixiv.net/member_illust.php?mode=medium&illust_id=62086450)

This is [the original image](62086450_p3.jpg) of the following reduced image.


### 1. Bilinear interpolation

It is a standard interpolation method of QuickViewer, and it is enough for many images as it is. It will look similar to Windows Photo Viewer.

![1 Bilinear interpolation](shurink-1-bilinear.png)


### 2. Bicubic, Spline and Lanczos interpolation by CPU

With these options the page is shrunk by the CPU instead of being scaled by the
view while it draws. It is usually the best quality, especially in Japanese
manga. Because they do not use the functions of the GPU, they work normally on
an old PC.

You may feel dissatisfied if drawing is not too fast, but we still do our best :)

![2 Bicubic interpolation by CPU](shurink-4-bicubic-by-cpu.png)

## Comparison of reduction results

![1 of compared](compared1.png)
![2 of compared](compared2.png)
