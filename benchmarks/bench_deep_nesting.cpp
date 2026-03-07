#include "utils.hpp"
#include <beast_json/beast_json.hpp>
#include <glaze/glaze.hpp>

struct Node100 { int value; bool operator==(const Node100& o) const { return value == o.value; } };
BEAST_JSON_FIELDS(Node100, value)
template <> struct glz::meta<Node100> { using T = Node100; static constexpr auto value = object("value", &T::value); };

struct Node99 { Node100 child; bool operator==(const Node99& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node99, child)
template <> struct glz::meta<Node99> { using T = Node99; static constexpr auto value = object("child", &T::child); };

struct Node98 { Node99 child; bool operator==(const Node98& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node98, child)
template <> struct glz::meta<Node98> { using T = Node98; static constexpr auto value = object("child", &T::child); };

struct Node97 { Node98 child; bool operator==(const Node97& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node97, child)
template <> struct glz::meta<Node97> { using T = Node97; static constexpr auto value = object("child", &T::child); };

struct Node96 { Node97 child; bool operator==(const Node96& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node96, child)
template <> struct glz::meta<Node96> { using T = Node96; static constexpr auto value = object("child", &T::child); };

struct Node95 { Node96 child; bool operator==(const Node95& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node95, child)
template <> struct glz::meta<Node95> { using T = Node95; static constexpr auto value = object("child", &T::child); };

struct Node94 { Node95 child; bool operator==(const Node94& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node94, child)
template <> struct glz::meta<Node94> { using T = Node94; static constexpr auto value = object("child", &T::child); };

struct Node93 { Node94 child; bool operator==(const Node93& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node93, child)
template <> struct glz::meta<Node93> { using T = Node93; static constexpr auto value = object("child", &T::child); };

struct Node92 { Node93 child; bool operator==(const Node92& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node92, child)
template <> struct glz::meta<Node92> { using T = Node92; static constexpr auto value = object("child", &T::child); };

struct Node91 { Node92 child; bool operator==(const Node91& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node91, child)
template <> struct glz::meta<Node91> { using T = Node91; static constexpr auto value = object("child", &T::child); };

struct Node90 { Node91 child; bool operator==(const Node90& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node90, child)
template <> struct glz::meta<Node90> { using T = Node90; static constexpr auto value = object("child", &T::child); };

struct Node89 { Node90 child; bool operator==(const Node89& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node89, child)
template <> struct glz::meta<Node89> { using T = Node89; static constexpr auto value = object("child", &T::child); };

struct Node88 { Node89 child; bool operator==(const Node88& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node88, child)
template <> struct glz::meta<Node88> { using T = Node88; static constexpr auto value = object("child", &T::child); };

struct Node87 { Node88 child; bool operator==(const Node87& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node87, child)
template <> struct glz::meta<Node87> { using T = Node87; static constexpr auto value = object("child", &T::child); };

struct Node86 { Node87 child; bool operator==(const Node86& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node86, child)
template <> struct glz::meta<Node86> { using T = Node86; static constexpr auto value = object("child", &T::child); };

struct Node85 { Node86 child; bool operator==(const Node85& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node85, child)
template <> struct glz::meta<Node85> { using T = Node85; static constexpr auto value = object("child", &T::child); };

struct Node84 { Node85 child; bool operator==(const Node84& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node84, child)
template <> struct glz::meta<Node84> { using T = Node84; static constexpr auto value = object("child", &T::child); };

struct Node83 { Node84 child; bool operator==(const Node83& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node83, child)
template <> struct glz::meta<Node83> { using T = Node83; static constexpr auto value = object("child", &T::child); };

struct Node82 { Node83 child; bool operator==(const Node82& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node82, child)
template <> struct glz::meta<Node82> { using T = Node82; static constexpr auto value = object("child", &T::child); };

struct Node81 { Node82 child; bool operator==(const Node81& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node81, child)
template <> struct glz::meta<Node81> { using T = Node81; static constexpr auto value = object("child", &T::child); };

struct Node80 { Node81 child; bool operator==(const Node80& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node80, child)
template <> struct glz::meta<Node80> { using T = Node80; static constexpr auto value = object("child", &T::child); };

struct Node79 { Node80 child; bool operator==(const Node79& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node79, child)
template <> struct glz::meta<Node79> { using T = Node79; static constexpr auto value = object("child", &T::child); };

struct Node78 { Node79 child; bool operator==(const Node78& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node78, child)
template <> struct glz::meta<Node78> { using T = Node78; static constexpr auto value = object("child", &T::child); };

struct Node77 { Node78 child; bool operator==(const Node77& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node77, child)
template <> struct glz::meta<Node77> { using T = Node77; static constexpr auto value = object("child", &T::child); };

struct Node76 { Node77 child; bool operator==(const Node76& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node76, child)
template <> struct glz::meta<Node76> { using T = Node76; static constexpr auto value = object("child", &T::child); };

struct Node75 { Node76 child; bool operator==(const Node75& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node75, child)
template <> struct glz::meta<Node75> { using T = Node75; static constexpr auto value = object("child", &T::child); };

struct Node74 { Node75 child; bool operator==(const Node74& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node74, child)
template <> struct glz::meta<Node74> { using T = Node74; static constexpr auto value = object("child", &T::child); };

struct Node73 { Node74 child; bool operator==(const Node73& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node73, child)
template <> struct glz::meta<Node73> { using T = Node73; static constexpr auto value = object("child", &T::child); };

struct Node72 { Node73 child; bool operator==(const Node72& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node72, child)
template <> struct glz::meta<Node72> { using T = Node72; static constexpr auto value = object("child", &T::child); };

struct Node71 { Node72 child; bool operator==(const Node71& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node71, child)
template <> struct glz::meta<Node71> { using T = Node71; static constexpr auto value = object("child", &T::child); };

struct Node70 { Node71 child; bool operator==(const Node70& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node70, child)
template <> struct glz::meta<Node70> { using T = Node70; static constexpr auto value = object("child", &T::child); };

struct Node69 { Node70 child; bool operator==(const Node69& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node69, child)
template <> struct glz::meta<Node69> { using T = Node69; static constexpr auto value = object("child", &T::child); };

struct Node68 { Node69 child; bool operator==(const Node68& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node68, child)
template <> struct glz::meta<Node68> { using T = Node68; static constexpr auto value = object("child", &T::child); };

struct Node67 { Node68 child; bool operator==(const Node67& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node67, child)
template <> struct glz::meta<Node67> { using T = Node67; static constexpr auto value = object("child", &T::child); };

struct Node66 { Node67 child; bool operator==(const Node66& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node66, child)
template <> struct glz::meta<Node66> { using T = Node66; static constexpr auto value = object("child", &T::child); };

struct Node65 { Node66 child; bool operator==(const Node65& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node65, child)
template <> struct glz::meta<Node65> { using T = Node65; static constexpr auto value = object("child", &T::child); };

struct Node64 { Node65 child; bool operator==(const Node64& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node64, child)
template <> struct glz::meta<Node64> { using T = Node64; static constexpr auto value = object("child", &T::child); };

struct Node63 { Node64 child; bool operator==(const Node63& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node63, child)
template <> struct glz::meta<Node63> { using T = Node63; static constexpr auto value = object("child", &T::child); };

struct Node62 { Node63 child; bool operator==(const Node62& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node62, child)
template <> struct glz::meta<Node62> { using T = Node62; static constexpr auto value = object("child", &T::child); };

struct Node61 { Node62 child; bool operator==(const Node61& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node61, child)
template <> struct glz::meta<Node61> { using T = Node61; static constexpr auto value = object("child", &T::child); };

struct Node60 { Node61 child; bool operator==(const Node60& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node60, child)
template <> struct glz::meta<Node60> { using T = Node60; static constexpr auto value = object("child", &T::child); };

struct Node59 { Node60 child; bool operator==(const Node59& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node59, child)
template <> struct glz::meta<Node59> { using T = Node59; static constexpr auto value = object("child", &T::child); };

struct Node58 { Node59 child; bool operator==(const Node58& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node58, child)
template <> struct glz::meta<Node58> { using T = Node58; static constexpr auto value = object("child", &T::child); };

struct Node57 { Node58 child; bool operator==(const Node57& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node57, child)
template <> struct glz::meta<Node57> { using T = Node57; static constexpr auto value = object("child", &T::child); };

struct Node56 { Node57 child; bool operator==(const Node56& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node56, child)
template <> struct glz::meta<Node56> { using T = Node56; static constexpr auto value = object("child", &T::child); };

struct Node55 { Node56 child; bool operator==(const Node55& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node55, child)
template <> struct glz::meta<Node55> { using T = Node55; static constexpr auto value = object("child", &T::child); };

struct Node54 { Node55 child; bool operator==(const Node54& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node54, child)
template <> struct glz::meta<Node54> { using T = Node54; static constexpr auto value = object("child", &T::child); };

struct Node53 { Node54 child; bool operator==(const Node53& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node53, child)
template <> struct glz::meta<Node53> { using T = Node53; static constexpr auto value = object("child", &T::child); };

struct Node52 { Node53 child; bool operator==(const Node52& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node52, child)
template <> struct glz::meta<Node52> { using T = Node52; static constexpr auto value = object("child", &T::child); };

struct Node51 { Node52 child; bool operator==(const Node51& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node51, child)
template <> struct glz::meta<Node51> { using T = Node51; static constexpr auto value = object("child", &T::child); };

struct Node50 { Node51 child; bool operator==(const Node50& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node50, child)
template <> struct glz::meta<Node50> { using T = Node50; static constexpr auto value = object("child", &T::child); };

struct Node49 { Node50 child; bool operator==(const Node49& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node49, child)
template <> struct glz::meta<Node49> { using T = Node49; static constexpr auto value = object("child", &T::child); };

struct Node48 { Node49 child; bool operator==(const Node48& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node48, child)
template <> struct glz::meta<Node48> { using T = Node48; static constexpr auto value = object("child", &T::child); };

struct Node47 { Node48 child; bool operator==(const Node47& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node47, child)
template <> struct glz::meta<Node47> { using T = Node47; static constexpr auto value = object("child", &T::child); };

struct Node46 { Node47 child; bool operator==(const Node46& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node46, child)
template <> struct glz::meta<Node46> { using T = Node46; static constexpr auto value = object("child", &T::child); };

struct Node45 { Node46 child; bool operator==(const Node45& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node45, child)
template <> struct glz::meta<Node45> { using T = Node45; static constexpr auto value = object("child", &T::child); };

struct Node44 { Node45 child; bool operator==(const Node44& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node44, child)
template <> struct glz::meta<Node44> { using T = Node44; static constexpr auto value = object("child", &T::child); };

struct Node43 { Node44 child; bool operator==(const Node43& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node43, child)
template <> struct glz::meta<Node43> { using T = Node43; static constexpr auto value = object("child", &T::child); };

struct Node42 { Node43 child; bool operator==(const Node42& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node42, child)
template <> struct glz::meta<Node42> { using T = Node42; static constexpr auto value = object("child", &T::child); };

struct Node41 { Node42 child; bool operator==(const Node41& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node41, child)
template <> struct glz::meta<Node41> { using T = Node41; static constexpr auto value = object("child", &T::child); };

struct Node40 { Node41 child; bool operator==(const Node40& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node40, child)
template <> struct glz::meta<Node40> { using T = Node40; static constexpr auto value = object("child", &T::child); };

struct Node39 { Node40 child; bool operator==(const Node39& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node39, child)
template <> struct glz::meta<Node39> { using T = Node39; static constexpr auto value = object("child", &T::child); };

struct Node38 { Node39 child; bool operator==(const Node38& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node38, child)
template <> struct glz::meta<Node38> { using T = Node38; static constexpr auto value = object("child", &T::child); };

struct Node37 { Node38 child; bool operator==(const Node37& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node37, child)
template <> struct glz::meta<Node37> { using T = Node37; static constexpr auto value = object("child", &T::child); };

struct Node36 { Node37 child; bool operator==(const Node36& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node36, child)
template <> struct glz::meta<Node36> { using T = Node36; static constexpr auto value = object("child", &T::child); };

struct Node35 { Node36 child; bool operator==(const Node35& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node35, child)
template <> struct glz::meta<Node35> { using T = Node35; static constexpr auto value = object("child", &T::child); };

struct Node34 { Node35 child; bool operator==(const Node34& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node34, child)
template <> struct glz::meta<Node34> { using T = Node34; static constexpr auto value = object("child", &T::child); };

struct Node33 { Node34 child; bool operator==(const Node33& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node33, child)
template <> struct glz::meta<Node33> { using T = Node33; static constexpr auto value = object("child", &T::child); };

struct Node32 { Node33 child; bool operator==(const Node32& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node32, child)
template <> struct glz::meta<Node32> { using T = Node32; static constexpr auto value = object("child", &T::child); };

struct Node31 { Node32 child; bool operator==(const Node31& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node31, child)
template <> struct glz::meta<Node31> { using T = Node31; static constexpr auto value = object("child", &T::child); };

struct Node30 { Node31 child; bool operator==(const Node30& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node30, child)
template <> struct glz::meta<Node30> { using T = Node30; static constexpr auto value = object("child", &T::child); };

struct Node29 { Node30 child; bool operator==(const Node29& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node29, child)
template <> struct glz::meta<Node29> { using T = Node29; static constexpr auto value = object("child", &T::child); };

struct Node28 { Node29 child; bool operator==(const Node28& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node28, child)
template <> struct glz::meta<Node28> { using T = Node28; static constexpr auto value = object("child", &T::child); };

struct Node27 { Node28 child; bool operator==(const Node27& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node27, child)
template <> struct glz::meta<Node27> { using T = Node27; static constexpr auto value = object("child", &T::child); };

struct Node26 { Node27 child; bool operator==(const Node26& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node26, child)
template <> struct glz::meta<Node26> { using T = Node26; static constexpr auto value = object("child", &T::child); };

struct Node25 { Node26 child; bool operator==(const Node25& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node25, child)
template <> struct glz::meta<Node25> { using T = Node25; static constexpr auto value = object("child", &T::child); };

struct Node24 { Node25 child; bool operator==(const Node24& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node24, child)
template <> struct glz::meta<Node24> { using T = Node24; static constexpr auto value = object("child", &T::child); };

struct Node23 { Node24 child; bool operator==(const Node23& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node23, child)
template <> struct glz::meta<Node23> { using T = Node23; static constexpr auto value = object("child", &T::child); };

struct Node22 { Node23 child; bool operator==(const Node22& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node22, child)
template <> struct glz::meta<Node22> { using T = Node22; static constexpr auto value = object("child", &T::child); };

struct Node21 { Node22 child; bool operator==(const Node21& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node21, child)
template <> struct glz::meta<Node21> { using T = Node21; static constexpr auto value = object("child", &T::child); };

struct Node20 { Node21 child; bool operator==(const Node20& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node20, child)
template <> struct glz::meta<Node20> { using T = Node20; static constexpr auto value = object("child", &T::child); };

struct Node19 { Node20 child; bool operator==(const Node19& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node19, child)
template <> struct glz::meta<Node19> { using T = Node19; static constexpr auto value = object("child", &T::child); };

struct Node18 { Node19 child; bool operator==(const Node18& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node18, child)
template <> struct glz::meta<Node18> { using T = Node18; static constexpr auto value = object("child", &T::child); };

struct Node17 { Node18 child; bool operator==(const Node17& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node17, child)
template <> struct glz::meta<Node17> { using T = Node17; static constexpr auto value = object("child", &T::child); };

struct Node16 { Node17 child; bool operator==(const Node16& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node16, child)
template <> struct glz::meta<Node16> { using T = Node16; static constexpr auto value = object("child", &T::child); };

struct Node15 { Node16 child; bool operator==(const Node15& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node15, child)
template <> struct glz::meta<Node15> { using T = Node15; static constexpr auto value = object("child", &T::child); };

struct Node14 { Node15 child; bool operator==(const Node14& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node14, child)
template <> struct glz::meta<Node14> { using T = Node14; static constexpr auto value = object("child", &T::child); };

struct Node13 { Node14 child; bool operator==(const Node13& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node13, child)
template <> struct glz::meta<Node13> { using T = Node13; static constexpr auto value = object("child", &T::child); };

struct Node12 { Node13 child; bool operator==(const Node12& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node12, child)
template <> struct glz::meta<Node12> { using T = Node12; static constexpr auto value = object("child", &T::child); };

struct Node11 { Node12 child; bool operator==(const Node11& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node11, child)
template <> struct glz::meta<Node11> { using T = Node11; static constexpr auto value = object("child", &T::child); };

struct Node10 { Node11 child; bool operator==(const Node10& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node10, child)
template <> struct glz::meta<Node10> { using T = Node10; static constexpr auto value = object("child", &T::child); };

struct Node9 { Node10 child; bool operator==(const Node9& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node9, child)
template <> struct glz::meta<Node9> { using T = Node9; static constexpr auto value = object("child", &T::child); };

struct Node8 { Node9 child; bool operator==(const Node8& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node8, child)
template <> struct glz::meta<Node8> { using T = Node8; static constexpr auto value = object("child", &T::child); };

struct Node7 { Node8 child; bool operator==(const Node7& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node7, child)
template <> struct glz::meta<Node7> { using T = Node7; static constexpr auto value = object("child", &T::child); };

struct Node6 { Node7 child; bool operator==(const Node6& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node6, child)
template <> struct glz::meta<Node6> { using T = Node6; static constexpr auto value = object("child", &T::child); };

struct Node5 { Node6 child; bool operator==(const Node5& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node5, child)
template <> struct glz::meta<Node5> { using T = Node5; static constexpr auto value = object("child", &T::child); };

struct Node4 { Node5 child; bool operator==(const Node4& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node4, child)
template <> struct glz::meta<Node4> { using T = Node4; static constexpr auto value = object("child", &T::child); };

struct Node3 { Node4 child; bool operator==(const Node3& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node3, child)
template <> struct glz::meta<Node3> { using T = Node3; static constexpr auto value = object("child", &T::child); };

struct Node2 { Node3 child; bool operator==(const Node2& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node2, child)
template <> struct glz::meta<Node2> { using T = Node2; static constexpr auto value = object("child", &T::child); };

struct Node1 { Node2 child; bool operator==(const Node1& o) const { return child == o.child; } };
BEAST_JSON_FIELDS(Node1, child)
template <> struct glz::meta<Node1> { using T = Node1; static constexpr auto value = object("child", &T::child); };

int main() {
    bench::print_header("Deep Nesting (Depth 100) Benchmark");
    bench::print_table_header();

    Node1 original;
    original.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.child.value = 42;

    constexpr int iterations = 1000;
    std::vector<bench::Result> results;

    // beast_json
    {
        bench::Timer serialize_timer, deserialize_timer;
        std::string json_str;
        Node1 result;

        serialize_timer.start();
        for (int i = 0; i < iterations; ++i) {
            json_str = beast::write(original);
        }
        double serialize_ns = serialize_timer.elapsed_ns() / iterations;

        deserialize_timer.start();
        for (int i = 0; i < iterations; ++i) {
            result = beast::read<Node1>(json_str);
        }
        double deserialize_ns = deserialize_timer.elapsed_ns() / iterations;

        bool correct = (result == original);
        results.push_back({"beast_json (C++17)", deserialize_ns, serialize_ns, correct});
    }

    // Glaze
    {
        bench::Timer serialize_timer, deserialize_timer;
        std::string json_str;
        Node1 result;

        serialize_timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto ec = glz::write_json(original, json_str);
            if (ec) { std::cerr << "Glaze write error\n"; break; }
        }
        double serialize_ns = serialize_timer.elapsed_ns() / iterations;

        deserialize_timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto ec = glz::read_json(result, json_str);
            if (ec) { std::cerr << "Glaze read error\n"; break; }
        }
        double deserialize_ns = deserialize_timer.elapsed_ns() / iterations;

        bool correct = (result == original);
        results.push_back({"Glaze (C++23)", deserialize_ns, serialize_ns, correct});
    }

    for (const auto &r : results) { r.print(); }
    std::cout << "\n✓ Benchmark complete\n";
    return 0;
}
