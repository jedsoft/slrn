% This file sets the UTF-8 conversion table correctly for ISO Latin 2.
% It is an example of how set_utf8_conversion_table works.
% To use it, simply put "interpret latin2.sl" in your slrnrc file.

define set_utf8_to_latin2 ()
{
   variable table = Integer_Type[2,128];
   variable i;
   
   % initialize the right column
   for (i = 0; i < 128; i++)
     table[1,i] = i + 128;
   
   % now fill the left column
   table[0,0] = 0x0080;
   table[0,1] = 0x0081;
   table[0,2] = 0x0082;
   table[0,3] = 0x0083;
   table[0,4] = 0x0084;
   table[0,5] = 0x0085;
   table[0,6] = 0x0086;
   table[0,7] = 0x0087;
   table[0,8] = 0x0088;
   table[0,9] = 0x0089;
   table[0,10] = 0x008A;
   table[0,11] = 0x008B;
   table[0,12] = 0x008C;
   table[0,13] = 0x008D;
   table[0,14] = 0x008E;
   table[0,15] = 0x008F;
   table[0,16] = 0x0090;
   table[0,17] = 0x0091;
   table[0,18] = 0x0092;
   table[0,19] = 0x0093;
   table[0,20] = 0x0094;
   table[0,21] = 0x0095;
   table[0,22] = 0x0096;
   table[0,23] = 0x0097;
   table[0,24] = 0x0098;
   table[0,25] = 0x0099;
   table[0,26] = 0x009A;
   table[0,27] = 0x009B;
   table[0,28] = 0x009C;
   table[0,29] = 0x009D;
   table[0,30] = 0x009E;
   table[0,31] = 0x009F;
   table[0,32] = 0x00A0;
   table[0,33] = 0x0104;
   table[0,34] = 0x02D8;
   table[0,35] = 0x0141;
   table[0,36] = 0x00A4;
   table[0,37] = 0x013D;
   table[0,38] = 0x015A;
   table[0,39] = 0x00A7;
   table[0,40] = 0x00A8;
   table[0,41] = 0x0160;
   table[0,42] = 0x015E;
   table[0,43] = 0x0164;
   table[0,44] = 0x0179;
   table[0,45] = 0x00AD;
   table[0,46] = 0x017D;
   table[0,47] = 0x017B;
   table[0,48] = 0x00B0;
   table[0,49] = 0x0105;
   table[0,50] = 0x02DB;
   table[0,51] = 0x0142;
   table[0,52] = 0x00B4;
   table[0,53] = 0x013E;
   table[0,54] = 0x015B;
   table[0,55] = 0x02C7;
   table[0,56] = 0x00B8;
   table[0,57] = 0x0161;
   table[0,58] = 0x015F;
   table[0,59] = 0x0165;
   table[0,60] = 0x017A;
   table[0,61] = 0x02DD;
   table[0,62] = 0x017E;
   table[0,63] = 0x017C;
   table[0,64] = 0x0154;
   table[0,65] = 0x00C1;
   table[0,66] = 0x00C2;
   table[0,67] = 0x0102;
   table[0,68] = 0x00C4;
   table[0,69] = 0x0139;
   table[0,70] = 0x0106;
   table[0,71] = 0x00C7;
   table[0,72] = 0x010C;
   table[0,73] = 0x00C9;
   table[0,74] = 0x0118;
   table[0,75] = 0x00CB;
   table[0,76] = 0x011A;
   table[0,77] = 0x00CD;
   table[0,78] = 0x00CE;
   table[0,79] = 0x010E;
   table[0,80] = 0x0110;
   table[0,81] = 0x0143;
   table[0,82] = 0x0147;
   table[0,83] = 0x00D3;
   table[0,84] = 0x00D4;
   table[0,85] = 0x0150;
   table[0,86] = 0x00D6;
   table[0,87] = 0x00D7;
   table[0,88] = 0x0158;
   table[0,89] = 0x016E;
   table[0,90] = 0x00DA;
   table[0,91] = 0x0170;
   table[0,92] = 0x00DC;
   table[0,93] = 0x00DD;
   table[0,94] = 0x0162;
   table[0,95] = 0x00DF;
   table[0,96] = 0x0155;
   table[0,97] = 0x00E1;
   table[0,98] = 0x00E2;
   table[0,99] = 0x0103;
   table[0,100] = 0x00E4;
   table[0,101] = 0x013A;
   table[0,102] = 0x0107;
   table[0,103] = 0x00E7;
   table[0,104] = 0x010D;
   table[0,105] = 0x00E9;
   table[0,106] = 0x0119;
   table[0,107] = 0x00EB;
   table[0,108] = 0x011B;
   table[0,109] = 0x00ED;
   table[0,110] = 0x00EE;
   table[0,111] = 0x010F;
   table[0,112] = 0x0111;
   table[0,113] = 0x0144;
   table[0,114] = 0x0148;
   table[0,115] = 0x00F3;
   table[0,116] = 0x00F4;
   table[0,117] = 0x0151;
   table[0,118] = 0x00F6;
   table[0,119] = 0x00F7;
   table[0,120] = 0x0159;
   table[0,121] = 0x016F;
   table[0,122] = 0x00FA;
   table[0,123] = 0x0171;
   table[0,124] = 0x00FC;
   table[0,125] = 0x00FD;
   table[0,126] = 0x0163;
   table[0,127] = 0x02D9;
   
   set_utf8_conversion_table (table);
}

set_utf8_to_latin2 ();
