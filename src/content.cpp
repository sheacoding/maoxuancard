/**
 * [INPUT]: content/ 中已核对的输入，由 tools/prepare.ts 生成
 * [OUTPUT]: 30 条原文及篇名、卷次、主题与核对链接
 * [POS]: 只读固件资源，禁止手工修改生成数据
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "core.h"
namespace linkbit {
const Builtin Builtins[BuiltinCount]={
    {"没有调查，没有发言权","反对本本主义","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193005.htm","调查研究",1,2},
    {"调查就是解决问题","反对本本主义","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193005.htm","调查研究",1,2},
    {"实践、认识、再实践、再认识","实践论","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193707.htm","实践",1,3},
    {"通过实践而发现真理","实践论","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193707.htm","实践",1,7},
    {"不同质的矛盾，只有用不同质的方法才能解决。","矛盾论","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193708.htm","矛盾分析",1,6},
    {"从战争学习战争","中国革命战争的战略问题","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193612.htm","学习",1,3},
    {"重要的问题在善于学习","中国革命战争的战略问题","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193612.htm","学习",1,7},
    {"不解决方法问题，任务也只是瞎说一顿。","关心群众生活，注意工作方法","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19340127.htm","工作方法",1,5},
    {"星星之火，可以燎原","星星之火，可以燎原","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19300105.htm","实践",1,7},
    {"学习的敌人是自己的满足","中国共产党在民族战争中的地位","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19381014.htm","学习",2,0},
    {"兵民是胜利之本","论持久战","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-193805b.htm","群众路线",2,4},
    {"对技术精益求精","纪念白求恩","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19391221.htm","工作方法",2,2},
    {"排泄其糟粕，吸收其精华","新民主主义论","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-194001.htm","学习",2,5},
    {"关心他人比关心自己为重","反对自由主义","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19370907.htm","群众路线",2,4},
    {"利用矛盾，争取多数，反对少数，各个击破","论政策","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19401225.htm","矛盾分析",2,6},
    {"群众是真正的英雄","〈农村调查〉的序言和跋","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-194134.htm","调查研究",3,0},
    {"这种态度，就是实事求是的态度。","改造我们的学习","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19410519.htm","实事求是",3,5},
    {"对任何事情都要问一个为什么","整顿党的作风","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19420201.htm","实事求是",3,2},
    {"要向人民群众学习语言。","反对党八股","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19420208.htm","工作方法",3,2},
    {"从群众中来，到群众中去","关于领导方法的若干问题","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19430601.htm","群众路线",3,4},
    {"只要你说得对，我们就改正。","为人民服务","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19440908.htm","实事求是",3,1},
    {"我们用自己动手的方法，达到了丰衣足食的目的。","组织起来","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19431129.htm","实践",3,4},
    {"放下包袱和开动机器","学习和时局","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19440412.htm","学习",3,3},
    {"中国靠我们来建设，我们必须努力学习。","必须学会做经济工作","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19450110.htm","学习",3,7},
    {"人民，只有人民，才是创造世界历史的动力。","论联合政府","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19450424.htm","群众路线",3,4},
    {"下定决心，不怕牺牲，排除万难，去争取胜利。","愚公移山","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19450611.htm","实践",3,3},
    {"学会“弹钢琴”","党委会的工作方法","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19490313.htm","工作方法",4,6},
    {"胸中有“数”","党委会的工作方法","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19490313.htm","调查研究",4,2},
    {"胜利的取得是逐步的。","关于重庆谈判","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19451017.htm","实践",4,1},
    {"要使不懂得变成懂得，就要去做去看，这就是学习。","对晋绥日报编辑人员的谈话","https://www.marxists.org/chinese/maozedong/marxist.org-chinese-mao-19480402.htm","实践",4,0}
};
}
