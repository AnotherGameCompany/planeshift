# MySQL-Front Dump 1.16 beta
#
# Host: localhost Database: planeshift
#--------------------------------------------------------
# Server version 3.23.52-max-nt
#
# Table structure for table 'sc_waypoint_links'
#

CREATE TABLE `sc_waypoint_links` (
  `id` int(8) unsigned NOT NULL auto_increment,
  `name` varchar(30) NOT NULL DEFAULT '' ,
  `type` varchar(30) NOT NULL DEFAULT '' ,
  `wp1` int(8) unsigned NOT NULL default '0',
  `wp2` int(8) unsigned NOT NULL default '0',
  `flags` varchar(30) DEFAULT '' ,
  PRIMARY KEY  (`id`)
);

#
# Dumping data for table 'sc_waypoint_lists'
#
INSERT INTO `sc_waypoint_links` VALUES (1, '', 'LINEAR' ,13, 14, '');
INSERT INTO `sc_waypoint_links` VALUES (2, '', 'LINEAR' ,1, 2, '');
INSERT INTO `sc_waypoint_links` VALUES (3, '', 'LINEAR' ,1, 6, '');
INSERT INTO `sc_waypoint_links` VALUES (4, '', 'LINEAR' ,2, 3, '');
INSERT INTO `sc_waypoint_links` VALUES (5, '', 'LINEAR' ,20, 19, 'ONEWAY');
INSERT INTO `sc_waypoint_links` VALUES (6, 'Behind Blacksmith', 'LINEAR' ,3, 4, '');
INSERT INTO `sc_waypoint_links` VALUES (7, '', 'LINEAR' ,3, 11, '');
INSERT INTO `sc_waypoint_links` VALUES (8, '', 'LINEAR' ,4, 5, '');
INSERT INTO `sc_waypoint_links` VALUES (9, '', 'LINEAR' ,6, 5, 'ONEWAY');
INSERT INTO `sc_waypoint_links` VALUES (10, '', 'LINEAR' ,8, 5, '');
INSERT INTO `sc_waypoint_links` VALUES (11, '', 'LINEAR' ,8, 7, '');
INSERT INTO `sc_waypoint_links` VALUES (12, '', 'LINEAR' ,9, 7, '');
INSERT INTO `sc_waypoint_links` VALUES (13, '', 'LINEAR' ,10, 7, '');
INSERT INTO `sc_waypoint_links` VALUES (14, '', 'LINEAR' ,12, 10, '');
INSERT INTO `sc_waypoint_links` VALUES (15, '', 'LINEAR' ,6, 13, '');
INSERT INTO `sc_waypoint_links` VALUES (16, '', 'LINEAR' ,13, 10, '');
INSERT INTO `sc_waypoint_links` VALUES (17, '', 'LINEAR' ,3, 15, '');
INSERT INTO `sc_waypoint_links` VALUES (18, '', 'LINEAR' ,15, 16, 'NO_WANDER');
INSERT INTO `sc_waypoint_links` VALUES (19, '', 'LINEAR' ,16, 17, '');
INSERT INTO `sc_waypoint_links` VALUES (20, '', 'LINEAR' ,18, 17, '');
INSERT INTO `sc_waypoint_links` VALUES (21, '', 'LINEAR' ,19, 5, '');
INSERT INTO `sc_waypoint_links` VALUES (22, '', 'LINEAR' ,2, 20, '');
INSERT INTO `sc_waypoint_links` VALUES (23, 'Test Path 1', 'LINEAR' ,14, 14, 'ONEWAY');

