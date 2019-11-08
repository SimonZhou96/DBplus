# Database Management System


[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)

## Table of Contents

- [Security](#security)
- [Background](#background)
- [Usage](#usage)
- [API](#api)
- [Contributing](#contributing)

## Security
There is a hidden page to store the page counter information, which is invisible to the user level
Furthermore, in the level of catalog file. There are two system tables which are only visible for the system level.

## Background
Here is the herarchy that the DBplus system is using.
![image](https://user-images.githubusercontent.com/34784304/68452284-e536e380-01a6-11ea-8d75-ca83340d9fcc.png)
The DBMS consisted of Record-based File Management, Relation Manager, B+ tree-based Index Manager and a pull-based Query Engine.
### Any optional sections
## Usage
The same as using C++(make + the test cases you wanna run)

## API
File operation:FSEEK(),FREAD, FWRITE() and etc.

## Contributing

### Contributor
Simon Zhou
### 
Implemented Record-based File Management, Relation Manager, B+ tree-based Index Manager and a pull-based Query Engine.

Implemented memory-based interfaces to optimize the speed of accessing page data, previously the data was read by disk I/O, greatly reduced the I/O usage by 80 %

Utilized CI/CD concept, implemented the bug-free program by timely maintaining the code via writing functional test cases as well as deploying the integrated code to production-like environments
