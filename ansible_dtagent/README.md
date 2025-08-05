alias ansible="docker run -ti --rm -v ~/.ssh:/root/.ssh -v ~/.aws:/root/.aws -v $(pwd):/apps -w /apps alpine/ansible:2.17.0 ansible"
ansible <follow command>

alias ansible-playbook=" docker run -ti --rm -v ~/.ssh:/root/.ssh -v ~/.aws:/root/.aws -v $(pwd):/apps -w /apps alpine/ansible:2.17.0 ansible-playbook"
ansible-playbook -i inventory < follow command>

与所有服务器建立 SSH 密钥对认证 ./setup_ssh_keys.sh

确保 agent 文件已经放在 files/ 目录下。

根据你的环境，修改 inventory.ini 中的服务器列表和 manager_ip。

打开终端，进入 ansible_dtagent 目录。

运行以下命令：

ansible-playbook -i inventory.ini install_agent.yml
Ansible 会自动连接到 inventory.ini 中列出的所有服务器，并按顺序执行 Playbook 中定义的任务。你可以看到每个步骤的执行状态（ok, changed, failed）。