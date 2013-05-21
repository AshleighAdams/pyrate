require('pyrate')

-- Create a new thread/channel
local t = thread.create()

-- Pass a function to our thread
t:run(function()
	thread.sleep(1000)
	print('Task #1 done')
	return 1, 33, 7
end)

-- Wait until it is done, and fetch it's result
local x, y, z = t:join()
print(x, y, z)

-- Pass another function to the thread
t:run(function()
	thread.sleep(1000)
	print('Task #2 done')
	return 2, 66, 14
end)

print('Waiting for thread to finish ...')

-- Threads can handle only one function at a time.
-- Since the thread is busy, we have to wait until the previous function is done.
-- This is an indirect t:join, but without any values retrieved from the thread channel.
-- You may pass additional parameters to the t:run method; they are used as parameters for the function you've passed along.
t:run(function(a, b, c)
	thread.sleep(1000)
	print('Task #3 done')
	return a*3, b*3, c*3
end, x, y, z)

print('Waiting for results ...')

-- Even though there are results ready to be retrieved,
-- we have to wait until the thread context is unlocked.
-- Passing 3 as parameter, because I want the first 3 available.
print(t:join(3))

-- The previous t:join already waited for the 3rd function to finish,
-- so no waiting to do here.
-- t:join without any parameters grabs every available result.
print(t:join())
