typedef struct Node { int val; struct Node *next; } Node;
int while_loop(Node *head) {
  int sum = 0;
  while (head) { sum += head->val; head = head->next; }
  return sum;
}
