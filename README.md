# CSE-LAB

## Lab1 : Basic File System

In this lab, you will learn how to implement your own file system step by step. In Part1, you will implement an inode manager to support your file system. In Part2, you will start your file system implementation by getting some basic operations to work.

## Lab2 : RPC and Lock Server

This lab includes three parts. In Part1(60 points) you will use RPC to implement a single client file server. In Part2(60 points), you will implememnt a lock server and add locking to ensure that concurrent operations to the same file/directory from different yfs_clients occur one at a time. In Part3(80 points), you will use lock cache to improve system performance.

## Lab1 ++ : Tune Your FS!

In this lab, you should tune your YFS implementation in Lab1, and make it approximately match the performance of native file system. We use 'fxmark' benchmark to profile your YFS and the native file system. Your task is to make your YFS run correctly (30 points) and fast (60 points). And 10 points for an extra doc.

## Lab3 : Transactional Key-Value Database

In this lab, you will implement a simple key-value database. It supports get, put, del, and the most important feature, transaction. 

## Lab4 : Data Cache and Beyond

Alice and Bob are two shopaholics who must visit shopping websites for bargains everyday. Since it is impossible to predict which item will be taken in advance, buyers such as Alice and Bob will have a copy of all available items locally. In reality, a shopping item cannot be physically cloned (otherwise it will break the law of conservation of mass!). So an item that Alice has purchased should be not visible to Bob any more. To serve as many customers as possible, the online seller wishes that Alice and Bob should not visit their website server very frequently. In order to serve more concurrent requests, the online buyer may use more physical resources to improve the server performance (e.g., by adding more CPUs).
