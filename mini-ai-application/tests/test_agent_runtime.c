#include "agent_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(nm) do { tests_run++; printf("  TEST: %s ... ", nm); } while(0)
#define PASS do { puts("PASS"); tests_passed++; } while(0)

void agent_register_default_tools(AgentRuntime *agent);

static bool mock_exec(const char *in, char *out, uint32_t ml) {
    (void)in; snprintf(out, ml, "mock"); return true;
}

static void t_agent_init(void) {
    TEST("agent_init");
    AgentRuntime a; agent_init(&a);
    assert(a.config.max_steps == AGENT_MAX_STEPS);
    assert(a.config.tool_count == 0);
    assert(!a.task_completed);
    PASS;
}

static void t_set_prompt(void) {
    TEST("set_prompt");
    AgentRuntime a; agent_init(&a);
    agent_set_prompt(&a, "Custom prompt.");
    assert(strcmp(a.config.system_prompt, "Custom prompt.") == 0);
    PASS;
}

static void t_reg_tool(void) {
    TEST("register_tool");
    AgentRuntime a; agent_init(&a);
    agent_register_tool(&a, "test", "desc", TOOL_CALC, mock_exec);
    assert(a.config.tool_count == 1);
    PASS;
}

static void t_add_msg(void) {
    TEST("add_message");
    AgentRuntime a; agent_init(&a);
    agent_add_message(&a, "user", "Hello");
    assert(a.config.msg_count == 1);
    PASS;
}

static void t_default_tools(void) {
    TEST("default_tools");
    AgentRuntime a; agent_init(&a);
    agent_register_default_tools(&a);
    assert(a.config.tool_count == 3);
    PASS;
}

static void t_null(void) {
    TEST("null safety");
    agent_init(NULL);
    agent_set_prompt(NULL, NULL);
    agent_register_tool(NULL, NULL, NULL, TOOL_SEARCH, NULL);
    agent_add_message(NULL, NULL, NULL);
    agent_react_loop(NULL, NULL);
    agent_clear_memory(NULL);
    agent_tot_init(NULL);
    agent_tot_search(NULL, NULL);
    agent_debate_init(NULL);
    agent_debate_vote(NULL);
    agent_tool_stats_init(NULL, 0);
    agent_reflect(NULL, NULL, NULL);
    PASS;
}

static void t_react_loop(void) {
    TEST("react_loop");
    AgentRuntime a; agent_init(&a);
    agent_register_default_tools(&a);
    agent_react_loop(&a, "What is 2+2?");
    assert(a.task_completed);
    assert(a.steps_taken >= 1);
    assert(strlen(a.final_answer) > 0);
    PASS;
}

static void t_decide_action(void) {
    TEST("decide_action");
    AgentRuntime a; agent_init(&a);
    assert(agent_decide_action(&a, "Start") == ACTION_REASON);
    PASS;
}

static void t_select_kw(void) {
    TEST("select_tool keyword");
    AgentRuntime a; agent_init(&a);
    agent_register_tool(&a, "search", "Search web", TOOL_SEARCH, mock_exec);
    agent_register_tool(&a, "calc", "Math", TOOL_CALC, mock_exec);
    uint32_t idx = agent_select_tool(&a, "I need to search");
    assert(idx < a.config.tool_count);
    PASS;
}

static void t_select_ucb(void) {
    TEST("select_tool UCB");
    AgentRuntime a; agent_init(&a);
    agent_register_tool(&a, "s", "Search", TOOL_SEARCH, mock_exec);
    agent_register_tool(&a, "c", "Calc", TOOL_CALC, mock_exec);
    ToolStats s[2]; agent_tool_stats_init(s, 2);
    agent_tool_update_stats(&s[1], 1, 0.9);
    uint32_t idx = agent_select_tool_ucb(&a, "q", s);
    assert(idx < 2);
    PASS;
}

static void t_multi_agent(void) {
    TEST("multi_agent");
    AgentRuntime ags[3];
    for (int i = 0; i < 3; i++) agent_init(&ags[i]);
    MultiAgentSystem m = { .agents = ags, .count = 3 };
    uint32_t aid = 99;
    agent_multi_delegate(&m, "Solve", &aid);
    assert(aid < 3);
    assert(ags[aid].task_completed);
    PASS;
}

static void t_clear_mem(void) {
    TEST("clear_memory");
    AgentRuntime a; agent_init(&a);
    agent_add_message(&a, "user", "test");
    a.steps_taken = 5; a.task_completed = true;
    agent_clear_memory(&a);
    assert(a.config.msg_count == 0);
    assert(a.steps_taken == 0);
    PASS;
}

static void t_conversation(void) {
    TEST("get_conversation");
    AgentRuntime a; agent_init(&a);
    agent_add_message(&a, "user", "Hi");
    agent_add_message(&a, "bot", "Hello");
    char buf[256];
    agent_get_conversation(&a, buf, sizeof(buf));
    assert(strstr(buf, "[user] Hi") != NULL);
    PASS;
}

static void t_tot(void) {
    TEST("tot_search");
    ToTState s;
    agent_tot_search(&s, "Design a cache");
    assert(s.count >= 1);
    assert(s.nodes[0].depth == 0);
    PASS;
}

static void t_reflect_good(void) {
    TEST("reflect good");
    AgentRuntime a; agent_init(&a);
    AgentReflection r;
    agent_reflect(&a, "A comprehensive answer with details", &r);
    assert(r.confidence > 0.5);
    assert(!r.should_retry);
    PASS;
}

static void t_reflect_bad(void) {
    TEST("reflect bad");
    AgentRuntime a; agent_init(&a);
    AgentReflection r;
    agent_reflect(&a, "bad", &r);
    assert(r.confidence < 0.5);
    assert(r.should_retry);
    PASS;
}

static void t_debate(void) {
    TEST("debate");
    AgentDebate db; agent_debate_init(&db);
    agent_debate_add_answer(&db, "A: sky blue");
    agent_debate_add_answer(&db, "B: sky appears blue");
    agent_debate_add_answer(&db, "A: sky blue");
    agent_debate_vote(&db);
    assert(db.answer_count == 3);
    assert(agent_debate_winner(&db) == 0);
    PASS;
}

static void t_safety_safe(void) {
    TEST("safety safe");
    assert(agent_safety_check("Nice weather") == SAFE);
    PASS;
}

static void t_safety_harmful(void) {
    TEST("safety harmful");
    assert(agent_safety_check("exploit security") == FLAG_HARMFUL);
    PASS;
}

static void t_safety_privacy(void) {
    TEST("safety privacy");
    assert(agent_safety_check("credit card number") == FLAG_PRIVACY);
    PASS;
}

int main(void) {
    puts("\n=== Agent Runtime Tests ===");
    puts("");
    t_agent_init(); t_set_prompt(); t_reg_tool(); t_add_msg();
    t_default_tools(); t_null(); t_react_loop(); t_decide_action();
    t_select_kw(); t_select_ucb(); t_multi_agent();
    t_clear_mem(); t_conversation(); t_tot();
    t_reflect_good(); t_reflect_bad(); t_debate();
    t_safety_safe(); t_safety_harmful(); t_safety_privacy();
    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
