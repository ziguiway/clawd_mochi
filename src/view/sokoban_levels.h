#pragma once

#include <Arduino.h>

// XSokoban classic levels 1-8.
// Source: https://github.com/davidjoffe/sokoban/blob/main/data/sokoban/levels/default.txt
// The source repository states that its default 90-level set is public domain.
namespace SokobanLevels {

static constexpr char LEVEL_1[] PROGMEM =
    "    #####\n"
    "    #   #\n"
    "    #$  #\n"
    "  ###  $##\n"
    "  #  $ $ #\n"
    "### # ## #   ######\n"
    "#   # ## #####  ..#\n"
    "# $  $          ..#\n"
    "##### ### #@##  ..#\n"
    "    #     #########\n"
    "    #######\n";

static constexpr char LEVEL_2[] PROGMEM =
    "############\n"
    "#..  #     ###\n"
    "#..  # $  $  #\n"
    "#..  #$####  #\n"
    "#..    @ ##  #\n"
    "#..  # #  $ ##\n"
    "###### ##$ $ #\n"
    "  # $  $ $ $ #\n"
    "  #    #     #\n"
    "  ############\n";

static constexpr char LEVEL_3[] PROGMEM =
    "        ########\n"
    "        #     @#\n"
    "        # $#$ ##\n"
    "        # $  $#\n"
    "        ##$ $ #\n"
    "######### $ # ###\n"
    "#....  ## $  $  #\n"
    "##...    $  $   #\n"
    "#....  ##########\n"
    "########\n";

static constexpr char LEVEL_4[] PROGMEM =
    "           ########\n"
    "           #  ....#\n"
    "############  ....#\n"
    "#    #  $ $   ....#\n"
    "# $$$#$  $ #  ....#\n"
    "#  $     $ #  ....#\n"
    "# $$ #$ $ $########\n"
    "#  $ #     #\n"
    "## #########\n"
    "#    #    ##\n"
    "#     $   ##\n"
    "#  $$#$$  @#\n"
    "#    #    ##\n"
    "###########\n";

static constexpr char LEVEL_5[] PROGMEM =
    "        #####\n"
    "        #   #####\n"
    "        # #$##  #\n"
    "        #     $ #\n"
    "######### ###   #\n"
    "#....  ## $  $###\n"
    "#....    $ $$ ##\n"
    "#....  ##$  $ @#\n"
    "#########  $  ##\n"
    "        # $ $  #\n"
    "        ### ## #\n"
    "          #    #\n"
    "          ######\n";

static constexpr char LEVEL_6[] PROGMEM =
    "######  ###\n"
    "#..  # ##@##\n"
    "#..  ###   #\n"
    "#..     $$ #\n"
    "#..  # # $ #\n"
    "#..### # $ #\n"
    "#### $ #$  #\n"
    "   #  $# $ #\n"
    "   # $  $  #\n"
    "   #  ##   #\n"
    "   #########\n";

static constexpr char LEVEL_7[] PROGMEM =
    "       #####\n"
    " #######   ##\n"
    "## # @## $$ #\n"
    "#    $      #\n"
    "#  $  ###   #\n"
    "### #####$###\n"
    "# $  ### ..#\n"
    "# $ $ $ ...#\n"
    "#    ###...#\n"
    "# $$ # #...#\n"
    "#  ### #####\n"
    "####\n";

static constexpr char LEVEL_8[] PROGMEM =
    "  ####\n"
    "  #  ###########\n"
    "  #    $   $ $ #\n"
    "  # $# $ #  $  #\n"
    "  #  $ $  #    #\n"
    "### $# #  #### #\n"
    "#@#$ $ $  ##   #\n"
    "#    $ #$#   # #\n"
    "#   $    $ $ $ #\n"
    "#####  #########\n"
    "  #      #\n"
    "  #      #\n"
    "  #......#\n"
    "  #......#\n"
    "  #......#\n"
    "  ########\n";

static constexpr const char* LEVELS[] PROGMEM = {
    LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4,
    LEVEL_5, LEVEL_6, LEVEL_7, LEVEL_8,
};

static constexpr uint8_t COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);

}  // namespace SokobanLevels
