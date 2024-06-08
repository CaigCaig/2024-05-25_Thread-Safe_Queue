#include <condition_variable> 
#include <functional> 
#include <iostream> 
#include <mutex> 
#include <queue> 
#include <thread> 
#include <vector>

using namespace std;

atomic<bool> flagDone{ false };

/*
Шаблонный класс safe_queue — реализация очереди, безопасной относительно одновременного доступа из нескольких потоков.
Минимально требуемые поля класса safe_queue:

очередь std::queue для хранения задач,
std::mutex для реализации блокировки,
std::condtional_variables для уведомлений.
Минимально требуемые методы класса safe_queue:

метод push — записывает в начало очереди новую задачу. При этом захватывает мьютекс, а после окончания операции нотифицируется условная переменная;
метод pop — находится в ожидании, пока не придёт уведомление на условную переменную. При нотификации условной переменной данные считываются из очереди;
*/

template <typename T>
class safe_queue
{
	T f;
	queue<std::function<void()>> workQueue;
	mutex workQueueMutex;
	condition_variable cv;
public:
	safe_queue(T func) : f(func) {}
	void push(T new_task)
	{
		std::lock_guard<std::mutex> lk(workQueueMutex);
		workQueue.push(std::move(new_task));
		cv.notify_one();
	};
	T pop(T task)
	{
		std::unique_lock<std::mutex> lk(workQueueMutex);
		cv.wait(lk, [] {workQueue.pop(task); });
		// std::lock_guard<std::mutex> lk(workQueueMutex);
		// workQueue.pop(task);
	};
};


/*
Класс thread_pool — реализация пула потоков.
Минимально требуемые поля класса thread_pool:

вектор потоков, которые инициализируют в конструкторе класса и уничтожают в деструкторе;
потокобезопасная очередь задач для хранения очереди работ;
остальные поля на усмотрение разработчика.
Минимально требуемые методы класса thread_pool:

метод work — выбирает из очереди очередную задачу и исполняет её. Этот метод передаётся конструктору потоков для исполнения;
метод submit — помещает в очередь задачу. В качестве аргумента метод может принимать или объект шаблона std::function, или объект шаблона package_task;
*/
class thread_pool {
public:
	// Конструктор для создания пула потоков с заданным количеством потоков
	thread_pool(size_t num_threads = thread::hardware_concurrency())
	{
		// Создание рабочих потоков

		for (int i = 0; i < num_threads - 3; i++)
		{
			threads.push_back(std::thread(&thread_pool::work, this));
		}
		/*
		for (auto& thread : threads)
		{
			thread.join();
		}
		*/

		/*
		for (size_t i = 0; i < num_threads; ++i) {
			threads.emplace_back([this] {
				while (true) {
					function<void()> task;
					// Разблокировка очереди перед выполнением задачи, чтобы другие потоки могли выполнять задачи в очереди
					{
						// Блокировка очереди для безопасного обмена данными
						unique_lock<mutex> lock(
							queue_mutex);

						// Ожидание, пока не появится задача для выполнения или пул не будет остановлен 
						cv.wait(lock, [this] {
							return !tasks.empty() || flagDone;
							});

						// выход из потока, если пул остановлен и задач нет 
						if (flagDone && tasks.empty()) {
							return;
						}

						// Получить следующую задачу из очереди
						task = move(tasks.front());
						tasks.pop();
					}

					task();
				}
				});
		}
		*/
	}

	// Деструктор для остановки пула потоков 
	~thread_pool()
	{
		{
			// Блокировка очереди, чтобы безопасно обновить флажок остановки
			unique_lock<mutex> lock(queue_mutex);
			flagDone = true;
		}

		// Оповещение всех потоков 
		cv.notify_all();

		// Объединение всех рабочих потоков для обеспечения выполнения ими своих задач 
		for (auto& thread : threads) {
			thread.join();
		}
	}

	// Постановка задачи в очередь для выполнения пулом потоков
	void submit(function<void()> task)
	{
		unique_lock<std::mutex> lock(queue_mutex);
		//tasks.emplace(move(task));
		work_queue.push(task);
		cv.notify_one();
	}

	void work()
	{
		queue_mutex.lock();
		std::cout << "Start working thread id: " << std::this_thread::get_id() << std::endl;
		queue_mutex.unlock();

		if (!work_queue.empty())
		{
			work_queue.pop();

		}

		/*
		while (!flagDone || !tasks.empty())
		{
			std::lock_guard<std::mutex> lockGuard{ queue_mutex };
			if (!tasks.empty())
			{
				auto task = tasks.front();
				tasks.pop();
				task();
			}
			else
			{
				std::this_thread::yield();
			}
		}
		*/
	}
private:
	// Вектор для хранения рабочих потоков 
	vector<thread> threads;

	// Очередь задач 
	queue<function<void()> > tasks;
	queue<function<void()> > work_queue;

	// Мьютекс для синхронизации доступа к общим данным
	mutex queue_mutex;

	// Переменная условия, сигнализирующая об изменениях в состоянии очереди задач 
	condition_variable cv;

	// Флаг, указывающий, следует ли останавливать пул потоков или нет 
	bool flagDone = false;
};

void func1()
{
	uint32_t counter = 0;
	std::this_thread::sleep_for(std::chrono::microseconds(200000));

	while (1) {
		std::cout << "Working thread id: " << std::this_thread::get_id() << " " << __FUNCTION__ << "..." << " Iteration: " << counter++ << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(1000000));
	}
}

void func2()
{
	uint32_t counter = 0;
	std::this_thread::sleep_for(std::chrono::microseconds(500000));

	while (1) {
		std::cout << "Working thread id: " << std::this_thread::get_id() << " " << __FUNCTION__ << "..." << " Iteration: " << counter++ << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(1000000));
	}
}

int main()
{
	auto cores = thread::hardware_concurrency();
	thread_pool pool(cores);

	// Постановка задач в очередь на выполнение 
	pool.submit(func1);
	pool.submit(func2);
	
	/*
	for (int i = 0; i < 5; ++i) {
		pool.submit([i] {
			cout << "Task " << i << " is running on thread "
				<< this_thread::get_id() << endl;
			// Simulate some work 
			this_thread::sleep_for(
				chrono::milliseconds(100));
			});
	}
	*/


	system("pause");
	return 0;
}

/*

Курсовой проект «Потокобезопасная очередь»
Пул потоков на базе потокобезопасной очереди.

Что нужно сделать:
Создать потокобезопасную очередь, хранящую функции, предназначенные для исполнения.
На основе этой очереди реализовать пул потоков.
Этот пул состоит из фиксированного числа рабочих потоков, равного количеству аппаратных ядер.
Когда у программы появляется какая-то работа, она вызывает функцию, которая помещает эту работу в очередь.
Рабочий поток забирает работу из очереди, выполняет указанную в ней задачу, после чего проверяет, есть ли в очереди другие работы.
Реализуемые классы
1. Класс thread_pool — реализация пула потоков.
Минимально требуемые поля класса thread_pool:

вектор потоков, которые инициализируют в конструкторе класса и уничтожают в деструкторе;
потокобезопасная очередь задач для хранения очереди работ;
остальные поля на усмотрение разработчика.
Минимально требуемые методы класса thread_pool:

метод work — выбирает из очереди очередную задачу и исполняет её. Этот метод передаётся конструктору потоков для исполнения;
метод submit — помещает в очередь задачу. В качестве аргумента метод может принимать или объект шаблона std::function, или объект шаблона package_task;
остальные методы на усмотрение разработчика.
2. Шаблонный класс safe_queue — реализация очереди, безопасной относительно одновременного доступа из нескольких потоков.
Минимально требуемые поля класса safe_queue:

очередь std::queue для хранения задач,
std::mutex для реализации блокировки,
std::condtional_variables для уведомлений.
Минимально требуемые методы класса safe_queue:

метод push — записывает в начало очереди новую задачу. При этом захватывает мьютекс, а после окончания операции нотифицируется условная переменная;
метод pop — находится в ожидании, пока не придёт уведомление на условную переменную. При нотификации условной переменной данные считываются из очереди;
остальные методы на усмотрение разработчика.
Алгоритм работы
Объявить объект класса thread_pool.
Описать несколько тестовых функций, выводящих в консоль своё имя.
Раз в секунду класть в пул одновременно 2 функции и проверять их исполнение.
Инструкция для выполнения курсового проекта
Выполняйте работу в GitHub.
Скопированную ссылку с вашей курсовой работой нужно отправить на проверку. Для этого перейдите в личный кабинет на сайте netology.ru, в поле комментария к курсовой вставьте скопированную ссылку и отправьте работу на проверку.
Работу можно сдавать частями.
Критерии оценки курсового проекта
В личном кабинете прикреплена ссылка с кодом курсового проекта.
В ссылке содержится код, который при запуске выполняет описанный в задании алгоритм.

*/